/*
 * Parity check: the same thirteen configurations tools/dsp_check.cpp renders,
 * run through the JavaScript port, printed in the same format.
 *
 *     node web/parity.mjs
 *
 * Compare against:
 *
 *     ./build/dsp_check_artefacts/Release/dsp_check
 *
 * Expect agreement to about two parts in a thousand, not to the digit: the C++
 * engine carries floats and JavaScript carries doubles, so the reverb's
 * feedback loops and the high-Q bells accumulate rounding differently. A sign
 * flip on a dc column reading +/-0.00000 is the same nothing.
 *
 * Anything larger than that is the port having drifted, which means the
 * prototyper is lying about what the plug-in would do — the only way this rig
 * can fail badly.
 */

import { PARAMS, INDEX, defaultPatch, gather, toNorm } from './params.js';
import { renderHit } from './render.js';

const SR = 48000;
const BLOCK = 128;
const SECONDS = 3;

/* dsp_check sends note-off at block 4. */
const GATE_MS = (4 * BLOCK) / SR * 1000;

const set = (patch, id, value) => { patch.knobs[INDEX[id]] = toNorm(PARAMS[INDEX[id]].range, value); };
const setNorm = (patch, id, n) => { patch.knobs[INDEX[id]] = n; };

const OFF = -60;

const cases = [
  ['default (sine only)', () => {}],

  ['all four shapes', p => {
    setNorm(p, 'lvltri', 1); setNorm(p, 'lvlsaw', 1);
    setNorm(p, 'lvlfold', 1); setNorm(p, 'fold', 1);
  }],

  ['fold swept by env', p => {
    setNorm(p, 'lvlsine', 0); setNorm(p, 'lvlfold', 1);
    setNorm(p, 'fold', 0.2); set(p, 'foldenv', 0.8);
  }],

  ['filter at full reso', p => {
    setNorm(p, 'lvlsaw', 1); set(p, 'cutoff', 120);
    setNorm(p, 'reso', 1); set(p, 'filtenv', 1);
  }],

  ['FM only, index 12', fmOnly],
  ['FM noise modulator', p => { fmOnly(p); p.choices.op2wave = 2; }],
  ['FM noise carrier', p => { fmOnly(p); p.choices.op1wave = 2; }],

  ['tape cranked', tapeCranked],
  ['tape cranked, no voice', p => { tapeCranked(p); set(p, 'sublevel', OFF); }],

  ['cab + biggest room', cabAndRoom],
  ['EQ at the extremes', eqExtremes],
  ['everything at once', everything],
  ['  and limited', p => { everything(p); p.toggles.limiter = true; }],
];

function fmOnly(p) {
  set(p, 'sublevel', OFF); set(p, 'fmlevel', 0);
  set(p, 'fmindex', 12); set(p, 'op2ratio', 16);
}

function tapeCranked(p) { setNorm(p, 'drive', 1); setNorm(p, 'hiss', 1); }

function cabAndRoom(p) {
  setNorm(p, 'cabmix', 1); setNorm(p, 'roommix', 1);
  setNorm(p, 'roomsize', 1); setNorm(p, 'roomdamp', 0);
}

function eqExtremes(p) {
  set(p, 'locut', 600); set(p, 'hicut', 600);
  for (const b of [1, 2, 3]) { set(p, `bell${b}g`, 18); set(p, `bell${b}q`, 12); }
}

function everything(p) {
  setNorm(p, 'lvltri', 1); setNorm(p, 'lvlsaw', 1);
  setNorm(p, 'lvlfold', 1); setNorm(p, 'fold', 1);
  setNorm(p, 'lvlsaw', 1); set(p, 'cutoff', 120);
  setNorm(p, 'reso', 1); set(p, 'filtenv', 1);
  fmOnly(p);
  set(p, 'sublevel', 12); set(p, 'fmlevel', 12);
  tapeCranked(p); cabAndRoom(p); eqExtremes(p);
  set(p, 'outlevel', 12);
}

console.log(`Stor (web port) — ${cases.length} configurations, one note each, ` +
            `${SECONDS} s at ${SR} Hz\n`);

let allFinite = true;

for (const [name, setup] of cases) {
  const patch = defaultPatch();
  setup(patch);

  const hit = renderHit(gather(patch), {
    note: 36, velocity: 1, gateMs: GATE_MS, sampleRate: SR, seconds: SECONDS,
  });

  let peak = 0, sumSq = 0, sum = 0, lastLoud = 0, finite = true;
  for (let i = 0; i < hit.L.length; i++) {
    const s = hit.L[i];
    if (!Number.isFinite(s)) finite = false;
    const a = Math.abs(s);
    if (a > peak) peak = a;
    if (a > 0.001) lastLoud = i;
    sumSq += s * s; sum += s;
  }

  const n = hit.L.length;
  const rms = Math.sqrt(sumSq / n);
  const dc = sum / n;
  const tail = lastLoud * 1000 / SR;

  allFinite = allFinite && finite;

  console.log(
    name.padEnd(26) +
    ` peak ${peak.toFixed(3).padStart(6)}` +
    `   rms ${rms.toFixed(4).padStart(6)}` +
    `   tail ${tail.toFixed(1).padStart(7)} ms` +
    `   dc ${(dc >= 0 ? '+' : '') + dc.toFixed(5).padStart(7)}` +
    `   ${finite ? 'ok' : 'NOT FINITE'}`);
}

console.log(`\n${allFinite ? 'all finite' : 'SOMETHING WENT NON-FINITE'}`);
