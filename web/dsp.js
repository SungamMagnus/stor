/*
 * Stor — the engine, ported from Source/Dsp.h and Source/StorEngine.cpp.
 *
 * A kick is monophonic and one-shot, so this renders a whole hit offline into
 * a buffer rather than running in an AudioWorklet: the DSP is identical, there
 * is nothing to glitch, and a 4-second render costs a few milliseconds. The
 * cost is that a knob moved mid-hit lands on the next hit, which is what a
 * drum machine does anyway.
 *
 * Keep this in step with the C++. Where the two disagree, the C++ is right.
 */

export const TWO_PI = Math.PI * 2;

export const clamp01 = v => (v < 0 ? 0 : v > 1 ? 1 : v);
export const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
export const dbToGain = db => Math.pow(10, db * 0.05);

/* Cheaper than tanh and close enough through the range that matters; exactly
   +/-1 past the knee rather than asymptotic to it. */
export function softClip(x) {
  if (x <= -1.5) return -1;
  if (x >= 1.5) return 1;
  return x - (4 / 27) * x * x * x;
}

/* ── Noise ───────────────────────────────────────────────────────────────
 * A 32-bit xorshift. The same sequence every run, which is what makes two
 * renders of the same patch comparable. */
export class Noise {
  constructor(seed = 0x1234567) { this.s = (seed | 1) >>> 0; }
  next() {
    let s = this.s;
    s = (s ^ (s << 13)) >>> 0;
    s = (s ^ (s >>> 17)) >>> 0;
    s = (s ^ (s << 5)) >>> 0;
    this.s = s;
    return (s | 0) * 4.656613e-10;
  }
}

/* ── Envelopes ───────────────────────────────────────────────────────────
 * Exponential segments, not linear ones. Each stage runs a one-pole towards a
 * target past where it stops, so the curve through the useful part is the
 * steep part and the stage still ends in finite time. */
const ATTACK_TARGET = 1.22, UNDERSHOOT = 0.04;
const IDLE = 0, ATTACK = 1, DECAY = 2, SUSTAIN = 3, RELEASE = 4;

export class Adsr {
  constructor() {
    this.sr = 96000;
    this.stage = IDLE; this.value = 0;
    this.aMs = 1; this.dMs = 200; this.s = 0; this.rMs = 60;
    this.set(this.aMs, this.dMs, this.s, this.rMs);
  }

  prepare(sr) { this.sr = sr; this.set(this.aMs, this.dMs, this.s, this.rMs); this.reset(); }
  reset() { this.stage = IDLE; this.value = 0; }

  coeff(ms) {
    const tau = Math.max(ms, 0.05) * 0.001 / 3;
    return 1 - Math.exp(-1 / (tau * this.sr));
  }

  set(a, d, s, r) {
    this.aMs = a; this.dMs = d; this.s = s; this.rMs = r;
    this.ac = this.coeff(a); this.dc = this.coeff(d); this.rc = this.coeff(r);
  }

  noteOn() { this.stage = ATTACK; }

  /* With no sustain there is nothing for a release to release: the decay is
     already on its way to zero, and cutting it short would mean the length of
     the drum came from how long the key was held. */
  noteOff() { if (this.stage !== IDLE && this.s > 1e-4) this.stage = RELEASE; }

  active() { return this.stage !== IDLE; }

  tick() {
    switch (this.stage) {
      case IDLE: return 0;
      case ATTACK:
        this.value += (ATTACK_TARGET - this.value) * this.ac;
        if (this.value >= 1) { this.value = 1; this.stage = DECAY; }
        break;
      case DECAY:
        this.value += (this.s - UNDERSHOOT - this.value) * this.dc;
        if (this.value <= this.s + 1e-4) {
          this.value = this.s;
          this.stage = this.s > 1e-4 ? SUSTAIN : IDLE;
        }
        break;
      case SUSTAIN:
        this.value = this.s;
        break;
      case RELEASE:
        this.value += (-UNDERSHOOT - this.value) * this.rc;
        if (this.value <= 1e-4) { this.value = 0; this.stage = IDLE; }
        break;
    }
    return this.value;
  }
}

/** The pitch drop. FALL is a time constant, not a finishing time. */
export class Fall {
  constructor() { this.sr = 96000; this.ms = 40; this.coeff = 0.999; this.value = 0; }
  prepare(sr) { this.sr = sr; this.setMs(this.ms); }
  setMs(ms) {
    this.ms = ms;
    this.coeff = Math.exp(-1 / (Math.max(ms, 0.1) * 0.001 * this.sr));
  }
  trigger() { this.value = 1; }
  reset() { this.value = 0; }
  tick() { const v = this.value; this.value *= this.coeff; return v; }
}

/* ── Oscillator ──────────────────────────────────────────────────────────
 * One phase accumulator feeding four shapes at once, because the mixer wants
 * them phase-locked. */
export class Osc {
  constructor() { this.sr = 96000; this.phase = 0; }
  prepare(sr) { this.sr = sr; this.phase = 0; }
  reset() { this.phase = 0; }

  /* One step's worth of correction, over the sample either side of the
     discontinuity at phase 0. */
  static polyBlep(t, dt) {
    if (dt <= 0) return 0;
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
  }

  /* A sine driven into sin() again, so the fold count grows smoothly rather
     than in the corners a reflecting folder puts in. Never leaves +/-1, so
     folding is a timbre control and not a level one. */
  static wavefold(x, depth) {
    return Math.sin(Math.PI * 0.5 * (1 + depth * 6) * x);
  }

  /** Writes sine, tri, saw, fold into `out`, advancing the phase once. */
  tick(hz, foldDepth, out) {
    const inc = hz / this.sr;

    out[0] = Math.sin(TWO_PI * this.phase);
    out[3] = Osc.wavefold(out[0], foldDepth);

    /* The saw is the one shape that breaks in value, so it is the one that
       needs correcting. */
    out[2] = 2 * this.phase - 1 - Osc.polyBlep(this.phase, inc);

    /* Naive triangle: continuous in value, breaking only in slope, so its
       partials fall off as 1/n^2 where the saw's fall off as 1/n. */
    out[1] = 4 * Math.abs(this.phase - 0.5) - 1;

    this.phase += inc;
    while (this.phase >= 1) this.phase -= 1;
  }
}

/* ── Filters ─────────────────────────────────────────────────────────────── */

/** Topology-preserving SVF: cutoff can be swept per sample without the
    coefficient-update artefacts a direct-form biquad gives. */
export class Svf {
  constructor() { this.sr = 96000; this.g = 0.1; this.k = 1; this.a1 = 1; this.s1 = 0; this.s2 = 0; }
  prepare(sr) { this.sr = sr; this.reset(); }
  reset() { this.s1 = 0; this.s2 = 0; }

  set(cutoffHz, q) {
    const nyq = this.sr * 0.5;
    const fc = clamp(cutoffHz, 10, nyq * 0.98);
    this.g = Math.tan(Math.PI * fc / this.sr);
    this.k = 1 / Math.max(q, 0.4);
    this.a1 = 1 / (1 + this.g * (this.g + this.k));
  }

  lowpass(x) {
    const hp = (x - (this.k + this.g) * this.s1 - this.s2) * this.a1;
    const v1 = this.g * hp + this.s1;
    const lp = this.g * v1 + this.s2;
    this.s1 = this.g * hp + v1;
    this.s2 = this.g * v1 + lp;
    return lp;
  }
}

/** Direct-form-II transposed biquad, with the RBJ designs the EQ needs. */
export class Biquad {
  constructor() { this.bypass(); this.z1 = 0; this.z2 = 0; }
  reset() { this.z1 = 0; this.z2 = 0; }
  bypass() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; }

  process(x) {
    const y = this.b0 * x + this.z1;
    this.z1 = this.b1 * x - this.a1 * y + this.z2;
    this.z2 = this.b2 * x - this.a2 * y;
    return y;
  }

  static clampFreq(sr, f) { return clamp(f, 10, sr * 0.475); }

  assign(b0, b1, b2, a0, a1, a2) {
    this.b0 = b0 / a0; this.b1 = b1 / a0; this.b2 = b2 / a0;
    this.a1 = a1 / a0; this.a2 = a2 / a0;
  }

  peak(sr, freq, gainDb, q) {
    const A = Math.pow(10, gainDb / 40);
    const w = TWO_PI * Biquad.clampFreq(sr, freq) / sr;
    const alpha = Math.sin(w) / (2 * Math.max(q, 0.05));
    const cw = Math.cos(w);
    this.assign(1 + alpha * A, -2 * cw, 1 - alpha * A,
                1 + alpha / A, -2 * cw, 1 - alpha / A);
  }

  lowShelf(sr, freq, gainDb, q = 0.707) {
    const A = Math.pow(10, gainDb / 40);
    const w = TWO_PI * Biquad.clampFreq(sr, freq) / sr;
    const cw = Math.cos(w), sw = Math.sin(w);
    const alpha = sw / (2 * q);
    const tsa = 2 * Math.sqrt(A) * alpha;
    this.assign(A * ((A + 1) - (A - 1) * cw + tsa),
                2 * A * ((A - 1) - (A + 1) * cw),
                A * ((A + 1) - (A - 1) * cw - tsa),
                (A + 1) + (A - 1) * cw + tsa,
                -2 * ((A - 1) + (A + 1) * cw),
                (A + 1) + (A - 1) * cw - tsa);
  }

  lowpassDesign(sr, freq, q = 0.707) {
    const w = TWO_PI * Biquad.clampFreq(sr, freq) / sr;
    const cw = Math.cos(w), alpha = Math.sin(w) / (2 * q);
    const b1 = 1 - cw;
    this.assign(b1 * 0.5, b1, b1 * 0.5, 1 + alpha, -2 * cw, 1 - alpha);
  }

  highpassDesign(sr, freq, q = 0.707) {
    const w = TWO_PI * Biquad.clampFreq(sr, freq) / sr;
    const cw = Math.cos(w), alpha = Math.sin(w) / (2 * q);
    const b0 = (1 + cw) * 0.5;
    this.assign(b0, -(1 + cw), b0, 1 + alpha, -2 * cw, 1 - alpha);
  }
}

/* ── Halfband decimator ──────────────────────────────────────────────────
 * Designed from a windowed sinc at construction rather than carried as a
 * table of constants. Linear phase, so the whole instrument is delayed by
 * exactly (taps - 1) / 4 host samples. */
export class Halfband {
  static taps = 31;
  static latency = (31 - 1) / 4;

  constructor() {
    const n = Halfband.taps, m = (n - 1) / 2;
    this.h = new Float32Array(n);
    this.z = new Float32Array(n);
    this.pos = 0;

    let sum = 0;
    for (let k = 0; k < n; k++) {
      const t = 0.5 * (k - m);
      const sinc = k === m ? 1 : Math.sin(Math.PI * t) / (Math.PI * t);
      const p = k / (n - 1);
      const win = 0.42 - 0.5 * Math.cos(TWO_PI * p) + 0.08 * Math.cos(2 * TWO_PI * p);
      this.h[k] = 0.5 * sinc * win;
      sum += this.h[k];
    }
    for (let k = 0; k < n; k++) this.h[k] /= sum;   // unity at DC
  }

  reset() { this.z.fill(0); this.pos = 0; }

  push(x) {
    this.z[this.pos] = x;
    this.pos = this.pos + 1 === Halfband.taps ? 0 : this.pos + 1;
  }

  /** Two samples in at twice the rate, one out at the host rate. */
  process(even, odd) {
    this.push(even);
    this.push(odd);

    const n = Halfband.taps;
    let acc = 0, i = this.pos;
    for (let k = 0; k < n; k++) {
      i = i === 0 ? n - 1 : i - 1;
      acc += this.h[k] * this.z[i];
    }
    return acc;
  }
}

/** Output limiter. Fast attack, slow release, then a hard clamp for the
    little a 2 ms attack lets through. */
export class Limiter {
  constructor(sr) {
    this.attack = 1 - Math.exp(-1 / (0.002 * sr));
    this.release = 1 - Math.exp(-1 / (0.15 * sr));
    this.env = 0; this.gain = 1; this.reduction = 1;
  }

  processSample(l, r) {
    const a = Math.max(Math.abs(l), Math.abs(r));
    this.env = a > this.env ? a : this.env + (a - this.env) * this.release;

    const target = this.env > 0.92 ? 0.92 / this.env : 1;
    this.gain += (target - this.gain) * (target < this.gain ? this.attack : this.release);

    if (this.gain < this.reduction) this.reduction = this.gain;
    return [clamp(l * this.gain, -1, 1), clamp(r * this.gain, -1, 1)];
  }

  readReduction() { const r = this.reduction; this.reduction = 1; return r; }
}

/* ── Cabinets and room geometry ──────────────────────────────────────────── */

const COMB_MS = [12.31, 15.73, 19.11, 22.87];
const ALLPASS_MS = [5.13, 1.71];
const RIGHT_SKEW = 1.037;
const ALLPASS_G = 0.5;

export const CABS = [
  { hpHz: 70, hpQ: 0.8, bumpHz: 95, bumpDb: 3.5, bumpQ: 1.2, scoopHz: 420, scoopDb: -6, scoopQ: 1, lpHz: 4200, lpQ: 0.9, makeupDb: 2 },
  { hpHz: 45, hpQ: 0.8, bumpHz: 68, bumpDb: 4.5, bumpQ: 1.3, scoopHz: 330, scoopDb: -5, scoopQ: 1, lpHz: 3200, lpQ: 0.9, makeupDb: 3 },
  { hpHz: 32, hpQ: 0.8, bumpHz: 48, bumpDb: 5.5, bumpQ: 1.4, scoopHz: 260, scoopDb: -4, scoopQ: 1, lpHz: 2400, lpQ: 0.9, makeupDb: 4 },
];

const SINE = 0, TRI = 1, NOISE = 2;

/**
 * The instrument. One monophonic voice through a fixed chain: tape, then the
 * cabinet, then the room that cabinet is standing in, and an EQ behind all of
 * it that gets to shape the room as well.
 */
export class StorEngine {
  constructor(sr) {
    this.sr = sr;
    this.sr2 = sr * 2;

    this.osc = new Osc(); this.osc.prepare(this.sr2);
    this.filter = new Svf(); this.filter.prepare(this.sr2);
    this.subEnv = new Adsr(); this.subEnv.prepare(this.sr2);
    this.fmEnv = new Adsr(); this.fmEnv.prepare(this.sr2);
    this.fall = new Fall(); this.fall.prepare(this.sr2);
    this.opPhase = [0, 0];
    this.opNoise = new Noise(0x1234567);
    this.hissNoise = new Noise(0x9e3779b9);
    this.shapes = new Float32Array(4);

    this.baseHz = 52; this.velGain = 1; this.held = 0;

    this.decim = new Halfband();
    this.bump = new Biquad();
    this.lossState = 0; this.lossCoeff = 1;
    this.hissHp = 0; this.hissGate = 0;
    this.hissRelease = Math.exp(-1 / (0.3 * this.sr2));
    this.dcX = 0; this.dcY = 0;
    this.dcCoeff = Math.exp(-TWO_PI * 15 / this.sr2);

    this.cabChain = [new Biquad(), new Biquad(), new Biquad(), new Biquad()];

    const maxRoom = Math.floor(sr * 0.06) + 4;
    const mk = () => ({ buf: new Float32Array(maxRoom), max: maxRoom, size: 1, pos: 0, store: 0 });
    this.combs = [[mk(), mk(), mk(), mk()], [mk(), mk(), mk(), mk()]];
    this.allpass = [[mk(), mk()], [mk(), mk()]];
    this.combFb = 0.7; this.combDamp = 0.4; this.combNorm = 0.25;
    this.roomHpState = 0;
    this.roomHpCoeff = 1 - Math.exp(-TWO_PI * 180 / sr);

    this.loCut = [new Biquad(), new Biquad()];
    this.hiCut = [new Biquad(), new Biquad()];
    this.bells = [[new Biquad(), new Biquad(), new Biquad()],
                  [new Biquad(), new Biquad(), new Biquad()]];
    this.loCutOn = false; this.hiCutOn = false;

    this.voiceEnv = 0;
    this.envCoeff = 1 - Math.exp(-1 / (0.03 * sr));
  }

  setParams(p) {
    this.p = p;

    this.subEnv.set(p.subA, p.subD, p.subS, p.subR);
    this.fmEnv.set(p.fmA, p.fmD, p.fmS, p.fmR);
    this.fall.setMs(p.fallMs);

    /* Open when clean, down to a cassette's 4.5 kHz when it is not. */
    this.lossCoeff = Math.exp(-TWO_PI * (18000 - 13500 * p.drive) / this.sr2);
    this.bump.lowShelf(this.sr2, 70, 5 * p.drive, 0.7);

    const c = CABS[clamp(p.cab | 0, 0, 2)];
    this.cabChain[0].highpassDesign(this.sr, c.hpHz, c.hpQ);
    this.cabChain[1].peak(this.sr, c.bumpHz, c.bumpDb, c.bumpQ);
    this.cabChain[2].peak(this.sr, c.scoopHz, c.scoopDb, c.scoopQ);
    this.cabChain[3].lowpassDesign(this.sr, c.lpHz, c.lpQ);
    this.cabMakeup = dbToGain(c.makeupDb);

    this.loCutOn = p.loCutHz > 20.5;
    this.hiCutOn = p.hiCutHz < 19900;

    for (let ch = 0; ch < 2; ch++) {
      if (this.loCutOn) this.loCut[ch].highpassDesign(this.sr, p.loCutHz, 0.707);
      else this.loCut[ch].bypass();

      if (this.hiCutOn) this.hiCut[ch].lowpassDesign(this.sr, p.hiCutHz, 0.707);
      else this.hiCut[ch].bypass();

      /* A bell at unity is bypassed rather than computed. */
      for (let b = 0; b < 3; b++) {
        if (Math.abs(p.bellG[b]) < 0.05) this.bells[ch][b].bypass();
        else this.bells[ch][b].peak(this.sr, p.bellF[b], p.bellG[b], p.bellQ[b]);
      }
    }

    /* Half length to one and a half, so the room goes from a booth to a
       live-ish drum room and no further. */
    const scale = 0.5 + p.roomSize;
    for (let ch = 0; ch < 2; ch++) {
      const skew = ch === 0 ? 1 : RIGHT_SKEW;
      for (let i = 0; i < 4; i++) {
        const d = this.combs[ch][i];
        d.size = clamp(Math.floor(COMB_MS[i] * scale * skew * 0.001 * this.sr), 1, d.max);
      }
      for (let i = 0; i < 2; i++) {
        const d = this.allpass[ch][i];
        d.size = clamp(Math.floor(ALLPASS_MS[i] * skew * 0.001 * this.sr), 1, d.max);
      }
    }

    this.combFb = 0.52 + 0.34 * p.roomSize;
    this.combDamp = 0.15 + 0.75 * p.roomDamp;

    /* Each comb's DC gain is 1/(1 - fb), so left alone a big room is not just
       longer, it is seven times louder. Normalising by the square root of that
       splits the difference: size mostly buys tail, and a little level. */
    this.combNorm = 0.25 * Math.sqrt(1 - this.combFb);
  }

  noteOn(midiNote, velocity) {
    this.held++;
    this.baseHz = this.p.tuneHz * Math.pow(2, (midiNote - 36) / 12);
    this.velGain = 1 - this.p.velAmount + this.p.velAmount * clamp01(velocity);

    /* Every hit starts at the same place in the cycle. */
    this.osc.reset();
    this.opPhase[0] = 0; this.opPhase[1] = 0;
    this.fall.trigger();
    this.subEnv.noteOn();
    this.fmEnv.noteOn();
  }

  noteOff() {
    this.held = Math.max(0, this.held - 1);
    if (this.held === 0) { this.subEnv.noteOff(); this.fmEnv.noteOff(); }
  }

  opSample(wave, phase) {
    if (wave === TRI) return 4 * Math.abs(phase - 0.5) - 1;
    if (wave === NOISE) return this.opNoise.next();
    return Math.sin(TWO_PI * phase);
  }

  renderVoice() {
    const p = this.p;
    const pitch = this.fall.tick();
    const subE = this.subEnv.tick();
    const fmE = this.fmEnv.tick();

    if (subE <= 0 && fmE <= 0) return 0;

    const hz = clamp(this.baseHz * Math.pow(2, p.bendSemis * pitch / 12), 10, this.sr2 * 0.45);

    /* ── Subtractive ─────────────────────────────────────────────────── */
    const foldDepth = clamp01(p.fold + p.foldEnv * subE);
    this.osc.tick(hz, foldDepth, this.shapes);
    const s = this.shapes;

    /* The mixer is a mixer: these sum, and are not normalised. */
    let sub = s[0] * p.lvl[0] + s[1] * p.lvl[1] + s[2] * p.lvl[2] + s[3] * p.lvl[3];

    if (p.cutoffHz < 17900 || p.resoQ > 0.71 || Math.abs(p.filtOctaves) > 0.02) {
      this.filter.set(p.cutoffHz * Math.pow(2, p.filtOctaves * subE), p.resoQ);
      sub = this.filter.lowpass(sub);
    }
    sub *= subE * p.subGain;

    /* ── FM: op 2 into op 1 ──────────────────────────────────────────── */
    let fm = 0;
    if (p.fmGain > 0) {
      const idx = p.index * (1 - p.indexEnv + p.indexEnv * fmE);
      const mod = this.opSample(p.wave[1], this.opPhase[1]) * idx;

      let car;
      if (p.wave[0] === NOISE) {
        /* Phase modulating noise does nothing — noise has no phase to move.
           The modulator rings it instead. */
        car = this.opNoise.next() * (0.5 + 0.5 * Math.cos(mod));
      } else {
        let ph = this.opPhase[0] + mod / TWO_PI;
        ph -= Math.floor(ph);
        car = this.opSample(p.wave[0], ph);
      }

      fm = car * fmE * p.fmGain;

      for (let o = 0; o < 2; o++) {
        this.opPhase[o] += hz * p.ratio[o] / this.sr2;
        this.opPhase[o] -= Math.floor(this.opPhase[o]);
      }
    }

    return (sub + fm) * this.velGain;
  }

  tape(x) {
    const p = this.p, drive = p.drive;

    if (drive > 0) {
      /* Head bump first: what the tape emphasises is what it then saturates,
         which is why a cassette gets fat before it gets dirty. */
      x = this.bump.process(x);

      const bias = drive * 0.12;
      x = softClip(x * (1 + drive * 15) + bias) - softClip(bias);

      /* Compensated by RMS, not by peak: saturation is supposed to collapse
         the crest factor. */
      x *= 1 / (1 + drive * 2);

      this.lossState += (x - this.lossState) * (1 - this.lossCoeff);
      x = this.lossState;
    }

    /* Hiss, gated by the voice so a loaded instance sitting still is silent. */
    const env = Math.max(this.subEnv.value, this.fmEnv.value);
    this.hissGate = env > this.hissGate ? env : this.hissGate * this.hissRelease;

    const hissAmp = drive * drive * p.hiss * 0.035 * this.hissGate;
    if (hissAmp > 0) {
      const n = this.hissNoise.next();
      this.hissHp += (n - this.hissHp) * 0.12;
      x += (n - this.hissHp) * hissAmp;
    }

    const y = x - this.dcX + this.dcCoeff * this.dcY;
    this.dcX = x; this.dcY = y;
    return y;
  }

  cabinet(x) {
    if (this.p.cabMix <= 0) return x;
    let c = x;
    for (let i = 0; i < 4; i++) c = this.cabChain[i].process(c);
    c *= this.cabMakeup;
    return x + (c - x) * this.p.cabMix;
  }

  eq(x, ch) {
    if (this.loCutOn) x = this.loCut[ch].process(x);
    for (let b = 0; b < 3; b++) x = this.bells[ch][b].process(x);
    if (this.hiCutOn) x = this.hiCut[ch].process(x);
    return x;
  }

  /** One host-rate frame. Returns [left, right]. */
  processSample() {
    const p = this.p;

    /* Two at the doubled rate, one out. The saturator lives inside it, being
       the only stage in the chain that generates what would fold. */
    const a = this.tape(this.renderVoice());
    const b = this.tape(this.renderVoice());
    const dry = this.cabinet(this.decim.process(a, b));

    this.voiceEnv += (Math.max(this.subEnv.value, this.fmEnv.value) - this.voiceEnv) * this.envCoeff;

    let outL = dry, outR = dry;

    if (p.roomMix > 0) {
      this.roomHpState += (dry - this.roomHpState) * this.roomHpCoeff;
      const send = dry - this.roomHpState;

      for (let ch = 0; ch < 2; ch++) {
        let wet = 0;

        for (let i = 0; i < 4; i++) {
          const d = this.combs[ch][i];
          const y = d.buf[d.pos];
          d.store += (y - d.store) * (1 - this.combDamp);
          d.buf[d.pos] = send + d.store * this.combFb;
          d.pos = d.pos + 1 >= d.size ? 0 : d.pos + 1;
          wet += y;
        }

        wet *= this.combNorm;

        for (let i = 0; i < 2; i++) {
          const d = this.allpass[ch][i];
          const y = d.buf[d.pos];
          d.buf[d.pos] = wet + y * ALLPASS_G;
          d.pos = d.pos + 1 >= d.size ? 0 : d.pos + 1;
          wet = y - wet * ALLPASS_G;
        }

        /* Added to the dry rather than crossfaded with it. */
        if (ch === 0) outL = dry + wet * p.roomMix;
        else outR = dry + wet * p.roomMix;
      }
    }

    return [this.eq(outL, 0) * p.outGain, this.eq(outR, 1) * p.outGain];
  }
}
