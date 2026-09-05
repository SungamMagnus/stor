/*
 * The parameter set, mirroring Source/Parameters.cpp — same ids, same ranges,
 * same defaults, same readout strings.
 *
 * The skew reproduces juce::NormalisableRange::setSkewForCentre, so a knob at
 * half travel lands on the same frequency here as it does in the plug-in. Get
 * this wrong and the panel lies about what the C++ would do, which is the one
 * thing a prototyping rig must not do.
 */

const skewForCentre = (lo, hi, centre) => Math.log(0.5) / Math.log((centre - lo) / (hi - lo));

/** A log-ish range, skewed so its geometric mean sits at half travel. */
function logRange(lo, hi) {
  return { lo, hi, skew: skewForCentre(lo, hi, Math.sqrt(lo * hi)) };
}

const linRange = (lo, hi) => ({ lo, hi, skew: 1 });

export function fromNorm(range, v) {
  if (range.skew !== 1 && v > 0) v = Math.exp(Math.log(v) / range.skew);
  return range.lo + (range.hi - range.lo) * v;
}

export function toNorm(range, value) {
  const p = (value - range.lo) / (range.hi - range.lo);
  return range.skew === 1 ? p : Math.pow(Math.max(p, 0), range.skew);
}

/* ── Readouts ────────────────────────────────────────────────────────────── */

const clamp01 = v => (v < 0 ? 0 : v > 1 ? 1 : v);

export const R = {
  pct: v => Math.round(clamp01(v) * 100) + '%',
  db: v => (Math.abs(v) < 0.05 ? '0.0 dB' : (v > 0 ? '+' : '') + v.toFixed(1) + ' dB'),
  level: v => (v <= -59.9 ? 'OFF' : R.db(v)),
  hz: v => (v >= 1000 ? (v / 1000).toFixed(v < 10000 ? 2 : 1) + ' kHz'
                      : (v < 100 ? v.toFixed(1) : Math.round(v)) + ' Hz'),
  ms: v => (v < 10 ? v.toFixed(2) + ' ms' : v < 100 ? v.toFixed(1) + ' ms' : Math.round(v) + ' ms'),
  semis: v => (v < 0.05 ? 'OFF' : '+' + v.toFixed(1) + ' st'),
  ratio: v => v.toFixed(v < 10 ? 2 : 1) + ':1',
  index: v => (v < 0.005 ? 'OFF' : v.toFixed(2) + ' rad'),
  octaves: v => (Math.abs(v) < 0.02 ? 'OFF' : (v > 0 ? '+' : '') + v.toFixed(2) + ' oct'),
  bipolar: v => (Math.abs(v) < 0.01 ? 'OFF' : (v > 0 ? '+' : '') + v.toFixed(2)),
  filtEnv: v => R.octaves(clamp01(Math.abs(v)) * Math.sign(v) * 6),
  loCut: v => (v > 20.5 ? R.hz(v) : 'OFF'),
  hiCut: v => (v < 19900 ? R.hz(v) : 'OPEN'),
  q: v => v.toFixed(2),
};

/* ── The table ───────────────────────────────────────────────────────────
 * In the order the signal runs, which is also the editor's knob order. */

const P = (id, range, def, readout) => ({ id, range, def, readout });

export const PARAMS = [
  /* Pitch */
  P('tune', logRange(20, 200), 52, R.hz),
  P('bend', linRange(0, 48), 30, R.semis),
  P('fall', logRange(1, 500), 22, R.ms),
  P('vel', linRange(0, 1), 0.6, R.pct),

  /* Subtractive */
  P('lvlsine', linRange(0, 1), 1, R.pct),
  P('lvltri', linRange(0, 1), 0, R.pct),
  P('lvlsaw', linRange(0, 1), 0, R.pct),
  P('lvlfold', linRange(0, 1), 0, R.pct),
  P('fold', linRange(0, 1), 0.25, R.pct),
  P('foldenv', linRange(-1, 1), 0, R.bipolar),
  P('cutoff', logRange(30, 18000), 18000, R.hz),
  P('reso', linRange(0, 1), 0, R.pct),
  P('filtenv', linRange(-1, 1), 0, R.filtEnv),
  P('suba', logRange(0.05, 200), 0.5, R.ms),
  P('subd', logRange(5, 4000), 320, R.ms),
  P('subs', linRange(0, 1), 0, R.pct),
  P('subr', logRange(5, 4000), 120, R.ms),
  P('sublevel', linRange(-60, 12), 0, R.level),

  /* FM */
  P('op1ratio', logRange(0.25, 16), 1, R.ratio),
  P('op2ratio', logRange(0.25, 16), 2, R.ratio),
  P('fmindex', linRange(0, 12), 2, R.index),
  P('fmindexenv', linRange(0, 1), 1, R.pct),
  P('fma', logRange(0.05, 200), 0.5, R.ms),
  P('fmd', logRange(5, 4000), 90, R.ms),
  P('fms', linRange(0, 1), 0, R.pct),
  P('fmr', logRange(5, 4000), 60, R.ms),
  P('fmlevel', linRange(-60, 12), -60, R.level),

  /* The chain */
  P('drive', linRange(0, 1), 0, R.pct),
  P('hiss', linRange(0, 1), 0.5, R.pct),
  P('cabmix', linRange(0, 1), 0, R.pct),
  P('roomsize', linRange(0, 1), 0.35, R.pct),
  P('roomdamp', linRange(0, 1), 0.55, R.pct),
  P('roommix', linRange(0, 1), 0, R.pct),
  P('locut', logRange(20, 600), 20, R.loCut),
  P('bell1f', logRange(20, 18000), 60, R.hz),
  P('bell1g', linRange(-18, 18), 0, R.db),
  P('bell1q', logRange(0.2, 12), 1, R.q),
  P('bell2f', logRange(20, 18000), 400, R.hz),
  P('bell2g', linRange(-18, 18), 0, R.db),
  P('bell2q', logRange(0.2, 12), 1, R.q),
  P('bell3f', logRange(20, 18000), 3000, R.hz),
  P('bell3g', linRange(-18, 18), 0, R.db),
  P('bell3q', logRange(0.2, 12), 1, R.q),
  P('hicut', logRange(600, 20000), 20000, R.hiCut),
  P('outlevel', linRange(-24, 12), 0, R.db),
];

export const INDEX = Object.fromEntries(PARAMS.map((p, i) => [p.id, i]));

/** Choices and the one boolean, which are not knobs and so are not in PARAMS. */
export const CHOICES = { op1wave: 0, op2wave: 0, cab: 2 };
export const TOGGLES = { limiter: false };

/** A fresh patch at the plug-in's defaults. */
export function defaultPatch() {
  const knobs = PARAMS.map(p => toNorm(p.range, p.def));
  return { knobs, choices: { ...CHOICES }, toggles: { ...TOGGLES } };
}

/** Engineering units, mirroring StorProcessor::gather(). */
export function gather(patch) {
  const v = id => fromNorm(PARAMS[INDEX[id]].range, patch.knobs[INDEX[id]]);
  const levelGain = db => (db <= -59.9 ? 0 : Math.pow(10, db / 20));

  return {
    tuneHz: v('tune'), bendSemis: v('bend'), fallMs: v('fall'), velAmount: v('vel'),

    lvl: [v('lvlsine'), v('lvltri'), v('lvlsaw'), v('lvlfold')],
    fold: v('fold'), foldEnv: v('foldenv'),
    cutoffHz: v('cutoff'),
    resoQ: 0.707 + clamp01(v('reso')) * 8.3,
    filtOctaves: Math.max(-1, Math.min(1, v('filtenv'))) * 6,
    subA: v('suba'), subD: v('subd'), subS: v('subs'), subR: v('subr'),
    subGain: levelGain(v('sublevel')),

    wave: [patch.choices.op1wave, patch.choices.op2wave],
    ratio: [v('op1ratio'), v('op2ratio')],
    index: v('fmindex'), indexEnv: v('fmindexenv'),
    fmA: v('fma'), fmD: v('fmd'), fmS: v('fms'), fmR: v('fmr'),
    fmGain: levelGain(v('fmlevel')),

    drive: v('drive'), hiss: v('hiss'),
    cab: patch.choices.cab, cabMix: v('cabmix'),
    roomSize: v('roomsize'), roomDamp: v('roomdamp'), roomMix: v('roommix'),
    loCutHz: v('locut'), hiCutHz: v('hicut'),
    bellF: [v('bell1f'), v('bell2f'), v('bell3f')],
    bellG: [v('bell1g'), v('bell2g'), v('bell3g')],
    bellQ: [v('bell1q'), v('bell2q'), v('bell3q')],

    outGain: Math.pow(10, v('outlevel') / 20),
    limiter: patch.toggles.limiter,
  };
}

/** The value a knob currently carries, in its own units. */
export const valueOf = (patch, i) => fromNorm(PARAMS[i].range, patch.knobs[i]);
export const readoutOf = (patch, i) => PARAMS[i].readout(valueOf(patch, i));

/* ── Persistence ─────────────────────────────────────────────────────────
 * The last patch survives a reload, in the browser this ran in only — it is
 * a convenience for picking up where you left off, not a save file. Nothing
 * here is trusted blind: a load fingerprints PARAMS/CHOICES/TOGGLES and
 * checks it against what is stored, so a schema that has since grown a knob,
 * dropped one, or reordered the array falls back to the plug-in defaults
 * instead of quietly misreading old numbers onto the wrong controls.
 */

const STORE_KEY = 'stor.patch.v1';
const fingerprint = () => [
  ...PARAMS.map(p => p.id),
  ...Object.keys(CHOICES),
  ...Object.keys(TOGGLES),
].join('|');

function validPatch(candidate) {
  if (!candidate || typeof candidate !== 'object') return false;
  if (candidate.fingerprint !== fingerprint()) return false;

  const { knobs, choices, toggles } = candidate;
  if (!Array.isArray(knobs) || knobs.length !== PARAMS.length) return false;
  if (!knobs.every(v => typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= 1)) return false;

  if (!choices || typeof choices !== 'object') return false;
  for (const k of Object.keys(CHOICES))
    if (typeof choices[k] !== 'number' || !Number.isInteger(choices[k])) return false;

  if (!toggles || typeof toggles !== 'object') return false;
  for (const k of Object.keys(TOGGLES))
    if (typeof toggles[k] !== 'boolean') return false;

  return true;
}

/** What was last saved, or null if there is nothing usable — corrupt JSON, a
    stale schema, and "never saved" all read the same way: use the defaults. */
export function loadPatch() {
  let raw;
  try { raw = localStorage.getItem(STORE_KEY); } catch { return null; }
  if (!raw) return null;

  let candidate;
  try { candidate = JSON.parse(raw); } catch { return null; }

  if (!validPatch(candidate)) return null;

  return {
    knobs: candidate.knobs.slice(),
    choices: { ...candidate.choices },
    toggles: { ...candidate.toggles },
  };
}

export function savePatch(patch) {
  try {
    localStorage.setItem(STORE_KEY, JSON.stringify({ fingerprint: fingerprint(), ...patch }));
  } catch {
    /* Private browsing, quota, or storage disabled — the rig still works,
       it just forgets when the tab closes. */
  }
}

export function clearSavedPatch() {
  try { localStorage.removeItem(STORE_KEY); } catch {}
}
