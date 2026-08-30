/*
 * The panel, ported from Source/Panel.h, Source/Panel.cpp and the layout
 * tables in Source/PluginEditor.cpp. Same 1280 x 1000 design space, same
 * palette, same geometry constants — so a change tried here can be moved into
 * the C++ by editing the same numbers.
 */

export const designW = 1280, designH = 1000;

export const hue = {
  paper:  '#f0ece2',
  ink:    '#1a1a17',
  coral:  '#ed8159',   // the subtractive engine
  teal:   '#52b0a4',   // the FM engine
  steel:  '#4f7ea8',   // everything after the two are summed
  violet: '#6b5bc4',   // modulation, and only modulation
  amber:  '#c08d16',   // the limiter
};

const ink = a => `rgba(26, 26, 23, ${a})`;
const alpha = (hex, a) => {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`;
};

/* ── Sizes ───────────────────────────────────────────────────────────────── */
export const rTune = 32, rBig = 26, rMed = 20, rMix = 18, rSml = 15, rTiny = 13, rTrim = 11;

/* ── The pitch bus ───────────────────────────────────────────────────────── */
export const pitchY = 136, midiX = 46, busEndX = 1000;
export const pitchLegSubX = 340, pitchLegFmX = 950;
export const fallMeterRect = { x: 700, y: 158, w: 180, h: 9 };

/* ── The two engines ─────────────────────────────────────────────────────── */
export const frameTop = 196, frameBot = 576;
export const subFrameL = 46, subFrameR = 636, fmFrameL = 660, fmFrameR = 1234;
export const envRow = 520, envLegY = 470;

export const subFanY = 266, subMixRow = 300, subSumY = 334, subShapeRow = 400;
export const subFoldX = 150, subCutoffX = 340, subResoX = 500, subOutX = 500;
export const voiceCol = i => 150 + 120 * i;

export const fmRow2 = 300, fmLoopY = 360, fmRow1 = 420;
export const fmInX = 700, fmOutX = 1000, fmLoopRightX = 1180;
export const fmRatioX = 930, fmIndexX = 1060;
export const opLatchX0 = 723, opLatchW = 44, opLatchStep = 48, opLatchH = 17;
export const opLatchEndX = opLatchX0 + opLatchStep * 2 + opLatchW;
export const fmCol = i => 730 + 105 * i;

export const opWaveRect = (op, wave) => ({
  x: opLatchX0 + opLatchStep * wave,
  y: (op === 0 ? fmRow1 : fmRow2) - opLatchH / 2,
  w: opLatchW, h: opLatchH,
});

/* ── Where the two engines meet, and the chain after ─────────────────────── */
export const mergeY = 628, mergeNodeX = 750, subLevelX = 600, fmLevelX = 900;
export const returnY = 706, chainLeftX = 80;
export const fxRow1 = 760, fxTurnX = 960, fxLinkY = 840, fxRow2 = 874;
export const outX = 1200, outMeterTop = 785;

export const cabRect = i => ({ x: 327 + 50 * i, y: fxRow1 - 17 / 2, w: 46, h: 17 });
export const bellCol = (bell, knob) => 210 + 190 * bell + 58 * knob;
export const limRect = { x: 961, y: fxRow2 - 8.5, w: 38, h: 17 };

/** Every trim sits off its control's upper right, clear of the value text. */
export const trimAt = (cx, cy, r) => ({ x: cx + r + 26, y: cy - r - 8 });

/* ── Primitives ──────────────────────────────────────────────────────────── */

const FONT = 'Menlo, Monaco, "DejaVu Sans Mono", "Courier New", monospace';

/* JUCE's Font height is ascent + descent; a canvas font size is the em. This
   is the ratio that lines the two up. */
const FONT_SCALE = 0.86;

export function setFont(ctx, size, bold = true) {
  ctx.font = `${bold ? 'bold ' : ''}${(size * FONT_SCALE).toFixed(2)}px ${FONT}`;
}

/** Centred in the rect both ways, like juce::Graphics::drawText. */
export function text(ctx, s, r, size, colour, just = 'centre', bold = true) {
  setFont(ctx, size, bold);
  ctx.fillStyle = colour;
  ctx.textBaseline = 'middle';
  ctx.textAlign = just === 'left' ? 'left' : just === 'right' ? 'right' : 'center';
  const x = just === 'left' ? r.x : just === 'right' ? r.x + r.w : r.x + r.w / 2;
  ctx.fillText(s, x, r.y + r.h / 2);
}

/** Letter-spaced run — the wordmark. */
export function tracked(ctx, s, r, size, colour, tracking, leftAlign = true) {
  setFont(ctx, size, true);
  ctx.fillStyle = colour;
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'left';

  let total = -tracking;
  for (const ch of s) total += ctx.measureText(ch).width + tracking;

  let x = leftAlign ? r.x : r.x + r.w / 2 - total / 2;
  for (const ch of s) {
    ctx.fillText(ch, x, r.y + r.h / 2);
    x += ctx.measureText(ch).width + tracking;
  }
}

/** A trace: a run of right-angle corners through the points given. */
export function wire(ctx, pts, colour, a, width = 1.4, dash = 0) {
  if (pts.length < 2) return;
  ctx.save();
  ctx.strokeStyle = alpha(colour, a);
  ctx.lineWidth = width;
  ctx.lineCap = 'butt';
  ctx.lineJoin = 'miter';
  ctx.setLineDash(dash > 0 ? [dash, dash] : []);
  ctx.beginPath();
  ctx.moveTo(pts[0][0], pts[0][1]);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
  ctx.stroke();
  ctx.restore();
}

export function node(ctx, x, y, colour, a, r = 3) {
  ctx.fillStyle = alpha(colour, a);
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  ctx.fill();
}

/** Lit when something is happening, outlined when it is not. */
export function lamp(ctx, cx, cy, on, colour) {
  const x = cx - 4.5, y = cy - 4.5;
  if (on) {
    ctx.fillStyle = colour;
    ctx.fillRect(x, y, 9, 9);
  } else {
    ctx.fillStyle = hue.paper;
    ctx.fillRect(x, y, 9, 9);
    ctx.strokeStyle = alpha(colour, 0.45);
    ctx.lineWidth = 1;
    ctx.strokeRect(x + 0.5, y + 0.5, 8, 8);
  }
}

export function terminal(ctx, x, y, label) {
  const r = 6.5;
  ctx.fillStyle = hue.paper;
  ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();
  ctx.strokeStyle = ink(0.7); ctx.lineWidth = 1.3;
  ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.stroke();
  text(ctx, label, { x: x - 45, y: y - 26, w: 90, h: 12 }, 8, ink(0.62));
}

/** A section, drawn as the box the components in it sit inside. */
export function frame(ctx, r, colour, title) {
  ctx.strokeStyle = alpha(colour, 0.34);
  ctx.lineWidth = 1;
  ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w, r.h);

  /* The title sits in the rule rather than under it, so the frame reads as one
     labelled thing rather than a box with a caption above it. */
  const w = 12 + 7.4 * title.length;
  ctx.fillStyle = hue.paper;
  ctx.fillRect(r.x + 14, r.y - 6, w, 12);
  text(ctx, title, { x: r.x + 20, y: r.y - 7, w, h: 13 }, 8.5, colour, 'left');
}

/* The pot sweep: 317 degrees, leaving a gap at the bottom where the pointer
   never goes. JUCE measures arcs from noon; canvas measures from three
   o'clock, hence the quarter turn. */
const SWEEP = 158.6 * Math.PI / 180;
const angleFor = n => -SWEEP + 2 * SWEEP * Math.max(0, Math.min(1, n)) - Math.PI / 2;

function arc(ctx, cx, cy, r, from, to, colour, width) {
  if (Math.abs(to - from) < 1e-4) return;
  ctx.strokeStyle = colour;
  ctx.lineWidth = width;
  ctx.lineCap = 'butt';
  ctx.beginPath();
  ctx.arc(cx, cy, r, Math.min(from, to), Math.max(from, to));
  ctx.stroke();
}

/** One circle, one arc, one pointer. Bipolar controls grow their arc from
    noon; unipolar ones from the anticlockwise stop. */
export function knob(ctx, cx, cy, r, norm, colour, bipolar = false) {
  /* Paper behind it, so a knob standing on a trace breaks the trace the way a
     component on a board does. */
  ctx.fillStyle = hue.paper;
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.fill();
  ctx.strokeStyle = ink(0.38); ctx.lineWidth = 1.2;
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();

  const aStart = angleFor(0), aEnd = angleFor(1), aNow = angleFor(norm);
  const track = r + 4.5;
  const w = r >= 24 ? 3.4 : r >= 16 ? 2.8 : 2.2;

  arc(ctx, cx, cy, track, aStart, aEnd, ink(0.13), w);
  arc(ctx, cx, cy, track, bipolar ? -Math.PI / 2 : aStart, aNow, colour, w);

  const px = Math.cos(aNow), py = Math.sin(aNow);
  ctx.strokeStyle = ink(0.85);
  ctx.lineWidth = r >= 24 ? 2 : 1.5;
  ctx.beginPath();
  ctx.moveTo(cx + px * r * 0.16, cy + py * r * 0.16);
  ctx.lineTo(cx + px * r * 0.78, cy + py * r * 0.78);
  ctx.stroke();

  /* A tick at the anticlockwise stop, so the sweep has a readable origin. */
  const tx = Math.cos(aStart), ty = Math.sin(aStart);
  ctx.strokeStyle = ink(0.28);
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(cx + tx * (r + 1.5), cy + ty * (r + 1.5));
  ctx.lineTo(cx + tx * (r + 6.5), cy + ty * (r + 6.5));
  ctx.stroke();
}

/** A control on the circuit: its dial, with designation and value above — or
    below, where the trace above it is spoken for. */
export function knobCell(ctx, cx, cy, r, norm, colour, label, value, bipolar = false, below = false) {
  knob(ctx, cx, cy, r, norm, colour, bipolar);

  const fs = r >= 30 ? 9.5 : r >= 18 ? 8.5 : 7.5;
  const ly = below ? cy + r + 6 : cy - r - 34;
  const vy = below ? cy + r + 19 : cy - r - 21;

  if (label) text(ctx, label, { x: cx - 70, y: ly, w: 140, h: 12 }, fs, ink(0.62));
  if (value) text(ctx, value, { x: cx - 70, y: vy, w: 140, h: 13 }, fs + 2, colour);
}

/** A modulation trim, tapped off its control's shoulder. */
export function trim(ctx, cx, cy, r, norm, bipolar = true) {
  const p = trimAt(cx, cy, r);

  /* A dashed leg back to its parent: this is a depth on that control, not a
     control of its own. */
  wire(ctx, [[cx + r * 0.72, cy - r * 0.72], [p.x - rTrim, p.y + rTrim]],
       hue.violet, 0.5, 1, 3);

  knob(ctx, p.x, p.y, rTrim, norm, hue.violet, bipolar);
}

export function latch(ctx, r, on, colour, label, size = 8) {
  ctx.fillStyle = on ? colour : hue.paper;
  ctx.fillRect(r.x, r.y, r.w, r.h);
  ctx.strokeStyle = on ? colour : alpha(colour, 0.55);
  ctx.lineWidth = 1.2;
  ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  text(ctx, label, r, size, on ? hue.paper : ink(0.62));
}

export function meter(ctx, r, value, colour, a = 0.75) {
  ctx.fillStyle = hue.paper;
  ctx.fillRect(r.x, r.y, r.w, r.h);
  ctx.strokeStyle = ink(0.3);
  ctx.lineWidth = 1;
  ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);

  const w = r.w * Math.max(0, Math.min(1, value));
  if (w > 0.5) {
    ctx.fillStyle = alpha(colour, a);
    ctx.fillRect(r.x + 1, r.y + 1, Math.max(0, w - 2), r.h - 2);
  }
}

/** A level meter stood on end, filling from the bottom. */
export function segMeter(ctx, cx, top, level, colour, segments = 7) {
  const w = 16, h = 6, gap = 3;
  const lit = Math.round(Math.max(0, Math.min(1, level)) * segments);

  for (let i = 0; i < segments; i++) {
    const r = { x: cx - w / 2, y: top + i * (h + gap), w, h };
    const on = segments - i <= lit;

    ctx.fillStyle = hue.paper;
    ctx.fillRect(r.x, r.y, r.w, r.h);

    /* The top segment is amber: it means you are nearly out of room. */
    const c = on ? (i === 0 ? hue.amber : colour) : ink(0.22);
    if (on) { ctx.fillStyle = c; ctx.fillRect(r.x, r.y, r.w, r.h); }
    else { ctx.strokeStyle = c; ctx.lineWidth = 1; ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1); }
  }
}
