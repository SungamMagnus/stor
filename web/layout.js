/*
 * The cell tables and paint routines, ported from Source/PluginEditor.cpp.
 * These are the same numbers as the C++ — if you move a knob here, move it
 * there.
 */

import * as P from './panel.js';
import { hue } from './panel.js';
import { INDEX, PARAMS, readoutOf } from './params.js';

const K = INDEX;
const ink = a => `rgba(26, 26, 23, ${a})`;

/** Where a knob sits and how it is drawn. Trims hang off their parent, and the
    parent draws them, so they are not in here. */
const cell = (id, cx, cy, r, label, bipolar = false, below = false) =>
  ({ i: K[id], cx, cy, r, label, bipolar, below });

export const PITCH_CELLS = [
  cell('tune', 150, P.pitchY, P.rTune, 'TUNE'),
  cell('bend', 290, P.pitchY, 24, 'BEND'),
  cell('fall', 396, P.pitchY, 24, 'FALL'),
  cell('vel', 492, P.pitchY, 16, 'VEL'),
];

export const SUB_CELLS = [
  cell('lvlsine', P.voiceCol(0), P.subMixRow, 18, 'SINE'),
  cell('lvltri', P.voiceCol(1), P.subMixRow, 18, 'TRI'),
  cell('lvlsaw', P.voiceCol(2), P.subMixRow, 18, 'SAW'),
  cell('lvlfold', P.voiceCol(3), P.subMixRow, 18, 'FOLD'),

  cell('fold', P.subFoldX, P.subShapeRow, P.rBig, 'FOLD DEPTH'),
  cell('cutoff', P.subCutoffX, P.subShapeRow, P.rBig, 'CUTOFF'),
  cell('reso', P.subResoX, P.subShapeRow, P.rMix, 'RESO'),

  cell('suba', P.voiceCol(0), P.envRow, P.rSml, 'ATTACK'),
  cell('subd', P.voiceCol(1), P.envRow, P.rSml, 'DECAY'),
  cell('subs', P.voiceCol(2), P.envRow, P.rSml, 'SUSTAIN'),
  cell('subr', P.voiceCol(3), P.envRow, P.rSml, 'RELEASE'),
];

export const FM_CELLS = [
  cell('op2ratio', P.fmRatioX, P.fmRow2, P.rMix, 'RATIO'),
  cell('op1ratio', P.fmRatioX, P.fmRow1, P.rMix, 'RATIO'),
  cell('fmindex', P.fmIndexX, P.fmRow2, P.rBig, 'INDEX'),

  cell('fma', P.fmCol(0), P.envRow, P.rSml, 'ATTACK'),
  cell('fmd', P.fmCol(1), P.envRow, P.rSml, 'DECAY'),
  cell('fms', P.fmCol(2), P.envRow, P.rSml, 'SUSTAIN'),
  cell('fmr', P.fmCol(3), P.envRow, P.rSml, 'RELEASE'),
];

export const CHAIN_CELLS = [
  cell('sublevel', P.subLevelX, P.mergeY, 24, 'SUB LEVEL', false, true),
  cell('fmlevel', P.fmLevelX, P.mergeY, 24, 'FM LEVEL', false, true),

  cell('drive', 140, P.fxRow1, P.rBig, 'TAPE DRIVE', false, true),
  cell('hiss', 240, P.fxRow1, 16, 'HISS', false, true),
  cell('cabmix', 550, P.fxRow1, P.rMed, 'CAB MIX', false, true),
  cell('roomsize', 700, P.fxRow1, P.rMed, 'ROOM SIZE', false, true),
  cell('roomdamp', 790, P.fxRow1, P.rMix, 'DAMPING', false, true),
  cell('roommix', 876, P.fxRow1, P.rMed, 'ROOM MIX', false, true),

  cell('locut', 120, P.fxRow2, P.rMed, 'LOW CUT', false, true),
  cell('bell1f', P.bellCol(0, 0), P.fxRow2, P.rSml, '1 FREQ', false, true),
  cell('bell1g', P.bellCol(0, 1), P.fxRow2, P.rMix, '1 GAIN', true, true),
  cell('bell1q', P.bellCol(0, 2), P.fxRow2, P.rTiny, '1 Q', false, true),
  cell('bell2f', P.bellCol(1, 0), P.fxRow2, P.rSml, '2 FREQ', false, true),
  cell('bell2g', P.bellCol(1, 1), P.fxRow2, P.rMix, '2 GAIN', true, true),
  cell('bell2q', P.bellCol(1, 2), P.fxRow2, P.rTiny, '2 Q', false, true),
  cell('bell3f', P.bellCol(2, 0), P.fxRow2, P.rSml, '3 FREQ', false, true),
  cell('bell3g', P.bellCol(2, 1), P.fxRow2, P.rMix, '3 GAIN', true, true),
  cell('bell3q', P.bellCol(2, 2), P.fxRow2, P.rTiny, '3 Q', false, true),
  cell('hicut', 790, P.fxRow2, P.rMed, 'HIGH CUT', false, true),

  cell('outlevel', 890, P.fxRow2, 24, 'OUTPUT', false, true),
];

/* The three trims: a modulation depth hanging off the control it moves. Each
   rides its own engine's amp envelope. */
export const TRIMS = [
  { i: K.foldenv, cx: P.subFoldX, cy: P.subShapeRow, r: P.rBig, bipolar: true },
  { i: K.filtenv, cx: P.subCutoffX, cy: P.subShapeRow, r: P.rBig, bipolar: true },
  { i: K.fmindexenv, cx: P.fmIndexX, cy: P.fmRow2, r: P.rBig, bipolar: false },
];

export const ALL_CELLS = [...PITCH_CELLS, ...SUB_CELLS, ...FM_CELLS, ...CHAIN_CELLS];

/** Hit boxes, in design space. Trims are smaller, and knobAt() lets the
    smaller target win where they overlap a shoulder. */
const box = (cx, cy, r) => ({ x: cx - r * 1.15, y: cy - r * 1.15, w: r * 2.3, h: r * 2.3 });

export const HIT_BOXES = (() => {
  const hits = new Array(PARAMS.length);
  for (const c of ALL_CELLS) hits[c.i] = box(c.cx, c.cy, c.r);
  for (const t of TRIMS) {
    const a = P.trimAt(t.cx, t.cy, t.r);
    hits[t.i] = box(a.x, a.y, P.rTrim);
  }
  return hits;
})();

const contains = (r, x, y) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;

export function knobAt(x, y) {
  let best = -1, bestArea = Infinity;
  for (let i = 0; i < HIT_BOXES.length; i++) {
    const r = HIT_BOXES[i];
    if (r && contains(r, x, y)) {
      const a = r.w * r.h;
      if (a < bestArea) { bestArea = a; best = i; }
    }
  }
  return best;
}

export const OP_WAVE_NAMES = ['SIN', 'TRI', 'NSE'];
export const CAB_NAMES = ['12"', '15"', '18"'];

/** Every multi-way choice position, as a hit target. */
export const RADIOS = (() => {
  const out = [];
  for (let op = 0; op < 2; op++)
    for (let w = 0; w < 3; w++)
      out.push({ id: op === 0 ? 'op1wave' : 'op2wave', index: w, rect: P.opWaveRect(op, w) });
  for (let c = 0; c < 3; c++)
    out.push({ id: 'cab', index: c, rect: P.cabRect(c) });
  return out;
})();

export function radioAt(x, y) {
  for (const r of RADIOS) {
    const e = { x: r.rect.x - 2, y: r.rect.y - 2, w: r.rect.w + 4, h: r.rect.h + 4 };
    if (contains(e, x, y)) return r;
  }
  return null;
}

export function limAt(x, y) {
  const r = P.limRect;
  return contains({ x: r.x - 3, y: r.y - 3, w: r.w + 6, h: r.h + 6 }, x, y);
}

/** Clicking the MIDI terminal plays one — the panel's own trigger. */
export function midiTerminalAt(x, y) {
  const dx = x - P.midiX, dy = y - P.pitchY;
  return dx * dx + dy * dy < 18 * 18;
}

/* ── Paint ───────────────────────────────────────────────────────────────── */

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const noteName = n => NOTE_NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 2);

export function paint(ctx, patch, meters) {
  ctx.fillStyle = hue.paper;
  ctx.fillRect(0, 0, P.designW, P.designH);

  ctx.strokeStyle = ink(0.22);
  ctx.lineWidth = 1;
  ctx.strokeRect(16.5, 16.5, P.designW - 32, P.designH - 32);

  paintPitch(ctx, patch, meters);
  paintSubtractive(ctx, patch);
  paintFm(ctx, patch);
  paintChain(ctx, patch, meters);

  P.tracked(ctx, 'WHOOMP', { x: 990, y: 940, w: 220, h: 22 }, 16, ink(0.55), 5, false);
  P.text(ctx, 'KICK SYNTHESISER', { x: 990, y: 964, w: 220, h: 12 }, 7.5, ink(0.38));
}

const drawCells = (ctx, patch, cells, colour) => {
  for (const c of cells)
    P.knobCell(ctx, c.cx, c.cy, c.r, patch.knobs[c.i], colour,
               c.label, readoutOf(patch, c.i), c.bipolar, c.below);
};

function paintPitch(ctx, patch, m) {
  /* The bus, and the two legs that hang the engines off it. Pitch draws in ink
     because it is not a section — it is the spine both sections are hung on. */
  P.wire(ctx, [[P.midiX + 9, P.pitchY], [P.busEndX, P.pitchY]], hue.ink, 0.55);
  P.wire(ctx, [[P.pitchLegSubX, P.pitchY], [P.pitchLegSubX, P.frameTop]], hue.ink, 0.55);
  P.wire(ctx, [[P.pitchLegFmX, P.pitchY], [P.pitchLegFmX, P.frameTop]], hue.ink, 0.55);
  P.node(ctx, P.pitchLegSubX, P.pitchY, hue.ink, 0.55);
  P.node(ctx, P.pitchLegFmX, P.pitchY, hue.ink, 0.55);

  P.terminal(ctx, P.midiX, P.pitchY, 'MIDI');
  P.lamp(ctx, 110, P.pitchY, m.sounding, hue.ink);

  drawCells(ctx, patch, PITCH_CELLS, `rgba(26, 26, 23, 0.78)`);

  /* Where the fall has got to, which is the one thing about this instrument
     you cannot read off a knob. */
  P.text(ctx, 'PITCH ENV', { x: 600, y: 154, w: 90, h: 12 }, 8, ink(0.62), 'left');
  P.meter(ctx, P.fallMeterRect, m.pitchEnv, hue.ink, 0.7);

  P.text(ctx, 'NOTE', { x: 1000, y: 154, w: 50, h: 12 }, 8, ink(0.62), 'left');
  P.text(ctx, m.noteText, { x: 1050, y: 153, w: 180, h: 13 }, 9.5, ink(0.78), 'left');
}

function paintSubtractive(ctx, patch) {
  P.frame(ctx, { x: P.subFrameL, y: P.frameTop, w: P.subFrameR - P.subFrameL, h: P.frameBot - P.frameTop },
          hue.coral, 'SUBTRACTIVE');

  /* One oscillator fanned to four shapes and summed. Drawn as a rail in and a
     rail out, because four free oscillators is what this is not. */
  P.wire(ctx, [[P.pitchLegSubX, P.frameTop], [P.pitchLegSubX, P.subFanY]], hue.coral, 0.45);
  P.wire(ctx, [[P.voiceCol(0), P.subFanY], [P.voiceCol(3), P.subFanY]], hue.coral, 0.45);
  P.wire(ctx, [[P.voiceCol(0), P.subSumY], [P.voiceCol(3), P.subSumY]], hue.coral, 0.45);

  for (let i = 0; i < 4; i++) {
    P.wire(ctx, [[P.voiceCol(i), P.subFanY], [P.voiceCol(i), P.subMixRow - 18]], hue.coral, 0.45);
    P.wire(ctx, [[P.voiceCol(i), P.subMixRow + 18], [P.voiceCol(i), P.subSumY]], hue.coral, 0.45);
    P.node(ctx, P.voiceCol(i), P.subSumY, hue.coral, 0.5, 2.4);
  }

  /* Sum, then fold, then the filter, then straight out of the bottom. */
  P.wire(ctx, [[P.subFoldX, P.subSumY], [P.subFoldX, P.subShapeRow - P.rBig]], hue.coral, 0.45);
  P.wire(ctx, [[P.subFoldX + P.rBig, P.subShapeRow], [P.subResoX, P.subShapeRow]], hue.coral, 0.45);
  P.wire(ctx, [[P.subOutX, P.subShapeRow + P.rMix], [P.subOutX, P.frameBot]], hue.coral, 0.45);

  drawCells(ctx, patch, SUB_CELLS, hue.coral);

  for (const t of TRIMS)
    if (t.i === K.foldenv || t.i === K.filtenv)
      P.trim(ctx, t.cx, t.cy, t.r, patch.knobs[t.i], t.bipolar);

  /* The envelope, on a dashed leg round the outside of the row: it is what
     gates the path, not a stage standing in it. */
  P.wire(ctx, [[P.voiceCol(3) + P.rSml + 8, P.envRow], [560, P.envRow],
               [560, P.envLegY], [P.subOutX, P.envLegY]], hue.coral, 0.4, 1.1, 4);
  P.node(ctx, P.subOutX, P.envLegY, hue.coral, 0.5, 2.4);
  P.text(ctx, 'AMP ENVELOPE', { x: P.voiceCol(0) - 34, y: P.envLegY - 19, w: 200, h: 12 },
         8, `rgba(237, 129, 89, 0.85)`, 'left');
}

function paintFm(ctx, patch) {
  P.frame(ctx, { x: P.fmFrameL, y: P.frameTop, w: P.fmFrameR - P.fmFrameL, h: P.frameBot - P.frameTop },
          hue.teal, 'FM');

  /* Pitch comes in once and runs down the left edge to both operators; each one
     multiplies it by its own ratio. */
  P.wire(ctx, [[P.pitchLegFmX, P.frameTop], [P.pitchLegFmX, 250],
               [P.fmInX, 250], [P.fmInX, P.fmRow1]], hue.teal, 0.45);
  P.wire(ctx, [[P.fmInX, P.fmRow2], [P.opLatchX0, P.fmRow2]], hue.teal, 0.45);
  P.wire(ctx, [[P.fmInX, P.fmRow1], [P.opLatchX0, P.fmRow1]], hue.teal, 0.45);
  P.node(ctx, P.fmInX, P.fmRow2, hue.teal, 0.5);

  P.wire(ctx, [[P.opLatchEndX, P.fmRow2], [P.fmRatioX - P.rMix, P.fmRow2]], hue.teal, 0.45);
  P.wire(ctx, [[P.opLatchEndX, P.fmRow1], [P.fmRatioX - P.rMix, P.fmRow1]], hue.teal, 0.45);

  /* Op two out through the index, then back down and into op one's phase —
     arriving on the same node its pitch does, which is exactly what phase
     modulation is. */
  P.wire(ctx, [[P.fmRatioX + P.rMix, P.fmRow2], [P.fmIndexX - P.rBig, P.fmRow2]], hue.teal, 0.45);
  P.wire(ctx, [[P.fmIndexX + P.rBig, P.fmRow2], [P.fmLoopRightX, P.fmRow2],
               [P.fmLoopRightX, P.fmLoopY], [P.fmInX, P.fmLoopY]], hue.teal, 0.45);
  P.node(ctx, P.fmInX, P.fmLoopY, hue.teal, 0.5);

  P.wire(ctx, [[P.fmRatioX + P.rMix, P.fmRow1], [P.fmOutX, P.fmRow1],
               [P.fmOutX, P.frameBot]], hue.teal, 0.45);

  P.text(ctx, 'OP 2', { x: P.opLatchX0, y: P.fmRow2 - 34, w: 44, h: 12 }, 8.5, hue.teal, 'left');
  P.text(ctx, 'OP 1', { x: P.opLatchX0, y: P.fmRow1 - 34, w: 44, h: 12 }, 8.5, hue.teal, 'left');

  for (const r of RADIOS)
    if (r.id !== 'cab')
      P.latch(ctx, r.rect, patch.choices[r.id] === r.index, hue.teal, OP_WAVE_NAMES[r.index], 7.5);

  drawCells(ctx, patch, FM_CELLS, hue.teal);

  for (const t of TRIMS)
    if (t.i === K.fmindexenv)
      P.trim(ctx, t.cx, t.cy, t.r, patch.knobs[t.i], t.bipolar);

  P.wire(ctx, [[P.fmCol(3) + P.rSml + 8, P.envRow], [1095, P.envRow],
               [1095, P.envLegY], [P.fmOutX, P.envLegY]], hue.teal, 0.4, 1.1, 4);
  P.node(ctx, P.fmOutX, P.envLegY, hue.teal, 0.5, 2.4);
  P.text(ctx, 'AMP ENVELOPE', { x: P.fmCol(0) - 34, y: P.envLegY - 19, w: 200, h: 12 },
         8, `rgba(82, 176, 164, 0.85)`, 'left');
}

function paintChain(ctx, patch, m) {
  /* Both engines onto one rail, then one line through everything after. */
  P.wire(ctx, [[P.subOutX, P.frameBot], [P.subOutX, P.mergeY],
               [P.fmOutX, P.mergeY], [P.fmOutX, P.frameBot]], hue.steel, 0.55);
  P.node(ctx, P.subOutX, P.mergeY, hue.steel, 0.55);
  P.node(ctx, P.fmOutX, P.mergeY, hue.steel, 0.55);
  P.node(ctx, P.mergeNodeX, P.mergeY, hue.steel, 0.6, 3.6);

  P.wire(ctx, [[P.mergeNodeX, P.mergeY], [P.mergeNodeX, P.returnY],
               [P.chainLeftX, P.returnY], [P.chainLeftX, P.fxRow1]], hue.steel, 0.55);
  P.wire(ctx, [[P.chainLeftX, P.fxRow1], [P.fxTurnX, P.fxRow1], [P.fxTurnX, P.fxLinkY],
               [P.chainLeftX, P.fxLinkY], [P.chainLeftX, P.fxRow2]], hue.steel, 0.55);
  P.wire(ctx, [[P.chainLeftX, P.fxRow2], [P.outX - 10, P.fxRow2]], hue.steel, 0.55);

  drawCells(ctx, patch, CHAIN_CELLS, hue.steel);

  P.text(ctx, 'CAB', { x: P.cabRect(0).x, y: P.fxRow1 + 24, w: 46, h: 12 }, 8, ink(0.62), 'left');

  for (const r of RADIOS)
    if (r.id === 'cab')
      P.latch(ctx, r.rect, patch.choices.cab === r.index, hue.steel, CAB_NAMES[r.index], 8);

  /* The limiter lives on the meter it guards. */
  P.latch(ctx, P.limRect, patch.toggles.limiter, hue.amber, 'LIM');
  P.lamp(ctx, 1025, P.fxRow2, m.limitGr > 0.02, hue.amber);

  P.terminal(ctx, P.outX, P.fxRow2, 'OUT');
  P.segMeter(ctx, P.outX, P.outMeterTop, m.outLevel, hue.steel);
}
