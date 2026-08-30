/*
 * Rendering one hit.
 *
 * The whole drum is rendered offline into a buffer and then played, rather
 * than run in real time. A kick is monophonic and one-shot, so nothing is lost
 * by it and quite a lot is gained: no worklet to load, nothing to glitch, and
 * the meters can be captured exactly rather than sampled at whatever rate the
 * screen happens to run at. A knob moved mid-hit lands on the next hit, which
 * is what a drum machine does anyway.
 */

import { WhoompEngine, Limiter, Halfband } from './dsp.js';

/** Meter frames captured while rendering, so playback can replay them. */
const METER_HZ = 240;

/**
 * How long to render. Long enough for the slowest thing still ringing: the two
 * envelopes, the room's tail, and the tape hiss gate, which runs on for about
 * a second after the voice has gone.
 */
function tailSeconds(p) {
  const env = Math.max(p.subA + p.subD + p.subR, p.fmA + p.fmD + p.fmR) / 1000;
  const room = p.roomMix > 0 ? 1.2 + p.roomSize * 1.4 : 0;
  const hiss = p.drive > 0 ? 1.2 : 0;
  return Math.min(9, Math.max(0.6, env + Math.max(room, hiss) + 0.4));
}

/**
 * Renders one hit. Returns interleaved-by-channel buffers plus the meter
 * track, mirroring what WhoompProcessor::processBlock publishes.
 */
export function renderHit(params, { note = 36, velocity = 1, gateMs = 100,
                                    sampleRate = 48000, seconds: forced = 0 } = {}) {
  /* `seconds` is forced only by the parity check, which has to render exactly
     the same window tools/dsp_check.cpp does. */
  const seconds = forced > 0 ? forced : tailSeconds(params);
  const n = Math.ceil(seconds * sampleRate);

  const engine = new WhoompEngine(sampleRate);
  engine.setParams(params);

  const limiter = new Limiter(sampleRate);
  const limiting = !!params.limiter;

  const L = new Float32Array(n);
  const Rc = new Float32Array(n);

  const gateAt = Math.min(n, Math.round(gateMs * 0.001 * sampleRate));

  const meterStride = Math.max(1, Math.round(sampleRate / METER_HZ));
  const meterCount = Math.ceil(n / meterStride);
  const meters = {
    stride: meterStride, sampleRate,
    pitchEnv: new Float32Array(meterCount),
    voiceEnv: new Float32Array(meterCount),
    outLevel: new Float32Array(meterCount),
    limitGr: new Float32Array(meterCount),
    sounding: new Uint8Array(meterCount),
  };

  /* The output follower from the processor: fast up, slow down. */
  const envCoeff = 1 - Math.exp(-1 / (sampleRate * 0.06));
  let outEnv = 0;

  engine.noteOn(note, velocity);

  let peak = 0, mi = 0;

  for (let i = 0; i < n; i++) {
    if (i === gateAt) engine.noteOff();

    let [l, r] = engine.processSample();
    if (limiting) [l, r] = limiter.processSample(l, r);

    L[i] = l; Rc[i] = r;

    const a = Math.abs(l);
    if (a > peak) peak = a;
    outEnv += (a - outEnv) * (a > outEnv ? 0.4 : envCoeff);

    if (i % meterStride === 0 && mi < meterCount) {
      meters.pitchEnv[mi] = engine.fall.value;
      meters.voiceEnv[mi] = engine.voiceEnv;
      meters.outLevel[mi] = Math.min(1, outEnv);
      meters.limitGr[mi] = limiting ? 1 - limiter.readReduction() : 0;
      meters.sounding[mi] = engine.subEnv.active() || engine.fmEnv.active() ? 1 : 0;
      mi++;
    }
  }

  return { L, R: Rc, sampleRate, seconds, peak, meters, latency: Halfband.latency };
}

/** Sample the captured meter track at a playback position, in seconds. */
export function metersAt(hit, t) {
  if (!hit) return { pitchEnv: 0, voiceEnv: 0, outLevel: 0, limitGr: 0, sounding: false };

  const m = hit.meters;
  const i = Math.floor(t * m.sampleRate / m.stride);
  if (i < 0 || i >= m.pitchEnv.length) {
    return { pitchEnv: 0, voiceEnv: 0, outLevel: 0, limitGr: 0, sounding: false };
  }

  return {
    pitchEnv: m.pitchEnv[i], voiceEnv: m.voiceEnv[i],
    outLevel: m.outLevel[i], limitGr: m.limitGr[i],
    sounding: m.sounding[i] === 1,
  };
}

/** A 16-bit stereo WAV of the rendered hit, for dropping into a sampler. */
export function toWav(hit) {
  const n = hit.L.length;
  const bytes = 44 + n * 4;
  const buf = new ArrayBuffer(bytes);
  const view = new DataView(buf);

  const str = (off, s) => { for (let i = 0; i < s.length; i++) view.setUint8(off + i, s.charCodeAt(i)); };

  str(0, 'RIFF'); view.setUint32(4, bytes - 8, true); str(8, 'WAVE');
  str(12, 'fmt '); view.setUint32(16, 16, true);
  view.setUint16(20, 1, true); view.setUint16(22, 2, true);
  view.setUint32(24, hit.sampleRate, true);
  view.setUint32(28, hit.sampleRate * 4, true);
  view.setUint16(32, 4, true); view.setUint16(34, 16, true);
  str(36, 'data'); view.setUint32(40, n * 4, true);

  let off = 44;
  for (let i = 0; i < n; i++) {
    const l = Math.max(-1, Math.min(1, hit.L[i]));
    const r = Math.max(-1, Math.min(1, hit.R[i]));
    view.setInt16(off, l < 0 ? l * 0x8000 : l * 0x7fff, true); off += 2;
    view.setInt16(off, r < 0 ? r * 0x8000 : r * 0x7fff, true); off += 2;
  }

  return new Blob([buf], { type: 'audio/wav' });
}
