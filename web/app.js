/*
 * The prototyper: the panel wired to the mouse, the keyboard, MIDI, and the
 * offline renderer. Web-only affordances live in the strip under the panel so
 * the faceplate itself stays exactly what the plug-in draws.
 */

import * as P from './panel.js';
import { hue } from './panel.js';
import * as L from './layout.js';
import { PARAMS, INDEX, defaultPatch, gather, toNorm, fromNorm, valueOf } from './params.js';
import { renderHit, metersAt, toWav } from './render.js';

const canvas = document.getElementById('panel');
const ctx = canvas.getContext('2d');
const scope = document.getElementById('scope');
const sctx = scope.getContext('2d');

const patch = defaultPatch();

const play = { note: 36, velocity: 1, gateMs: 100, auto: true };
let hit = null;             // the last render
let audioCtx = null;
let source = null;
let playStart = 0;
let playing = false;
let dirty = true;

const idle = { pitchEnv: 0, voiceEnv: 0, outLevel: 0, limitGr: 0, sounding: false };

/* ── Canvas sizing ───────────────────────────────────────────────────────── */

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const w = canvas.clientWidth, h = canvas.clientHeight;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
  dirty = true;
}

/** Client pixels to the 1280 x 1000 design space the layout is written in. */
function toDesign(e) {
  const r = canvas.getBoundingClientRect();
  return {
    x: (e.clientX - r.left) * P.designW / r.width,
    y: (e.clientY - r.top) * P.designH / r.height,
  };
}

/* ── Paint ───────────────────────────────────────────────────────────────── */

function frame() {
  const t = playing ? (audioCtx.currentTime - playStart) : 0;
  const m = playing ? metersAt(hit, t) : idle;

  if (playing && t > hit.seconds) { playing = false; dirty = true; }

  if (dirty || playing) {
    const dpr = canvas.width / P.designW;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    L.paint(ctx, patch, { ...m, noteText: `${L.noteName(play.note)}   ${noteHz()}` });
    dirty = false;
  }

  requestAnimationFrame(frame);
}

function noteHz() {
  const hz = valueOf(patch, INDEX.tune) * Math.pow(2, (play.note - 36) / 12);
  return hz >= 1000 ? (hz / 1000).toFixed(2) + ' kHz'
                    : (hz < 100 ? hz.toFixed(1) : Math.round(hz)) + ' Hz';
}

/* ── The scope ───────────────────────────────────────────────────────────
 * The rendered hit, drawn once per render. For a kick the first forty
 * milliseconds are the whole argument, so the view is logarithmic in time —
 * the transient gets room and the tail is still on screen. */

function drawScope() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const w = scope.clientWidth, h = scope.clientHeight;
  scope.width = Math.round(w * dpr);
  scope.height = Math.round(h * dpr);
  sctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  sctx.fillStyle = hue.paper;
  sctx.fillRect(0, 0, w, h);
  sctx.strokeStyle = 'rgba(26,26,23,0.25)';
  sctx.lineWidth = 1;
  sctx.strokeRect(0.5, 0.5, w - 1, h - 1);

  sctx.strokeStyle = 'rgba(26,26,23,0.15)';
  sctx.beginPath(); sctx.moveTo(0, h / 2); sctx.lineTo(w, h / 2); sctx.stroke();

  if (!hit) {
    sctx.fillStyle = 'rgba(26,26,23,0.4)';
    sctx.font = '9px Menlo, monospace';
    sctx.textAlign = 'center'; sctx.textBaseline = 'middle';
    sctx.fillText('no hit yet', w / 2, h / 2);
    return;
  }

  const n = hit.L.length;
  const mid = h / 2, amp = h / 2 - 2;

  sctx.fillStyle = 'rgba(79,126,168,0.85)';
  for (let px = 0; px < w; px++) {
    /* Logarithmic time: t = (10^(p*k) - 1) / (10^k - 1). */
    const k = 2.6;
    const a = Math.floor(n * (Math.pow(10, (px / w) * k) - 1) / (Math.pow(10, k) - 1));
    const b = Math.floor(n * (Math.pow(10, ((px + 1) / w) * k) - 1) / (Math.pow(10, k) - 1));

    let lo = 0, hiV = 0;
    for (let i = a; i < Math.max(b, a + 1) && i < n; i++) {
      const v = hit.L[i];
      if (v < lo) lo = v;
      if (v > hiV) hiV = v;
    }

    const y0 = mid - Math.max(-1, Math.min(1, hiV)) * amp;
    const y1 = mid - Math.max(-1, Math.min(1, lo)) * amp;
    sctx.fillRect(px, y0, 1, Math.max(1, y1 - y0));
  }

  sctx.fillStyle = 'rgba(26,26,23,0.55)';
  sctx.font = '8px Menlo, monospace';
  sctx.textAlign = 'left'; sctx.textBaseline = 'top';
  sctx.fillText(`${hit.seconds.toFixed(2)} s   peak ${hit.peak.toFixed(3)}` +
                (hit.peak > 1 ? '  OVER' : ''), 4, 3);
}

/* ── Sound ───────────────────────────────────────────────────────────────── */

function ensureAudio() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  if (audioCtx.state === 'suspended') audioCtx.resume();
  return audioCtx;
}

function trigger(velocity = play.velocity, note = play.note) {
  const ac = ensureAudio();

  const t0 = performance.now();
  hit = renderHit(gather(patch), {
    note, velocity, gateMs: play.gateMs, sampleRate: ac.sampleRate,
  });
  const ms = performance.now() - t0;

  const buf = ac.createBuffer(2, hit.L.length, hit.sampleRate);
  buf.copyToChannel(hit.L, 0);
  buf.copyToChannel(hit.R, 1);

  if (source) { try { source.stop(); } catch {} }
  source = ac.createBufferSource();
  source.buffer = buf;
  source.connect(ac.destination);
  source.start();

  playStart = ac.currentTime;
  playing = true;
  dirty = true;

  drawScope();
  status(`rendered ${hit.seconds.toFixed(2)} s in ${ms.toFixed(1)} ms · ` +
         `peak ${hit.peak.toFixed(3)}${hit.peak > 1 ? ' (over)' : ''} · ` +
         `latency ${hit.latency} samples`);
}

const statusEl = document.getElementById('status');
const status = s => { statusEl.textContent = s; };

/* ── Pointer ─────────────────────────────────────────────────────────────── */

let drag = null;

canvas.addEventListener('pointerdown', e => {
  const d = toDesign(e);

  const radio = L.radioAt(d.x, d.y);
  if (radio) {
    patch.choices[radio.id] = radio.index;
    dirty = true;
    if (play.auto) trigger();
    return;
  }

  if (L.limAt(d.x, d.y)) {
    patch.toggles.limiter = !patch.toggles.limiter;
    dirty = true;
    if (play.auto) trigger();
    return;
  }
  if (L.midiTerminalAt(d.x, d.y)) { trigger(); return; }

  const i = L.knobAt(d.x, d.y);
  if (i < 0) return;

  drag = { i, startNorm: patch.knobs[i], startY: d.y };
  canvas.setPointerCapture(e.pointerId);
});

canvas.addEventListener('pointermove', e => {
  if (!drag) return;
  const d = toDesign(e);
  const travel = L.TRIMS.some(t => t.i === drag.i) ? 90 : 100;
  const sens = e.shiftKey ? 0.22 : 1;
  const delta = (drag.startY - d.y) / travel;

  patch.knobs[drag.i] = Math.max(0, Math.min(1, drag.startNorm + delta * sens));
  dirty = true;
});

/* Auto: play again when a knob is let go. A kick is a quarter of a second, so
   the loop of move-something / hear-it wants to be one gesture, not two. */
const endDrag = () => {
  const was = drag;
  drag = null;
  if (was && play.auto) trigger();
};
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);

canvas.addEventListener('dblclick', e => {
  const d = toDesign(e);
  const i = L.knobAt(d.x, d.y);
  if (i < 0) return;
  patch.knobs[i] = toNorm(PARAMS[i].range, PARAMS[i].def);
  dirty = true;
  if (play.auto) trigger();
});

canvas.addEventListener('wheel', e => {
  const d = toDesign(e);
  const i = L.knobAt(d.x, d.y);
  if (i < 0) return;
  e.preventDefault();
  const gain = e.shiftKey ? 0.04 : 0.16;
  patch.knobs[i] = Math.max(0, Math.min(1, patch.knobs[i] - Math.sign(e.deltaY) * gain));
  dirty = true;
}, { passive: false });

/* ── Keyboard ────────────────────────────────────────────────────────────
 * Space plays the selected note; the bottom row walks up from C1, so the kick
 * and the toms above it are both one key away. */

const KEYS = 'zsxdcvgbhnjm';

window.addEventListener('keydown', e => {
  if (e.target !== document.body) return;

  if (e.code === 'Space') { e.preventDefault(); trigger(); return; }

  const k = KEYS.indexOf(e.key.toLowerCase());
  if (k >= 0 && !e.repeat) { play.note = 36 + k; syncControls(); trigger(); }
});

/* ── MIDI ────────────────────────────────────────────────────────────────── */

const midiEl = document.getElementById('midi');

if (navigator.requestMIDIAccess) {
  navigator.requestMIDIAccess().then(access => {
    const wire = () => {
      const names = [];
      for (const input of access.inputs.values()) {
        names.push(input.name);
        input.onmidimessage = m => {
          const [st, d1, d2] = m.data;
          if ((st & 0xf0) === 0x90 && d2 > 0) {
            play.note = d1;
            syncControls();
            trigger(d2 / 127, d1);
          }
        };
      }
      midiEl.textContent = names.length ? names.join(', ') : 'no devices';
    };
    access.onstatechange = wire;
    wire();
  }).catch(() => { midiEl.textContent = 'blocked'; });
} else {
  midiEl.textContent = 'unsupported';
}

/* ── The strip ───────────────────────────────────────────────────────────── */

const noteEl = document.getElementById('note');
const velEl = document.getElementById('vel');
const velOut = document.getElementById('velout');
const gateEl = document.getElementById('gate');
const gateOut = document.getElementById('gateout');

const autoEl = document.getElementById('auto');

function syncControls() {
  noteEl.textContent = L.noteName(play.note);
  velOut.textContent = Math.round(play.velocity * 127);
  gateOut.textContent = play.gateMs + ' ms';
  autoEl.setAttribute('aria-pressed', String(play.auto));
  autoEl.textContent = play.auto ? 'Auto on' : 'Auto off';
  dirty = true;
}

autoEl.addEventListener('click', () => { play.auto = !play.auto; syncControls(); });

document.getElementById('hit').addEventListener('click', () => trigger());
document.getElementById('noteup').addEventListener('click', () => {
  play.note = Math.min(96, play.note + 1); syncControls();
});
document.getElementById('notedown').addEventListener('click', () => {
  play.note = Math.max(12, play.note - 1); syncControls();
});

velEl.addEventListener('input', () => { play.velocity = +velEl.value / 127; syncControls(); });
gateEl.addEventListener('input', () => { play.gateMs = +gateEl.value; syncControls(); });

document.getElementById('reset').addEventListener('click', () => {
  const fresh = defaultPatch();
  patch.knobs = fresh.knobs;
  patch.choices = fresh.choices;
  patch.toggles = fresh.toggles;
  dirty = true;
  status('back to the plug-in defaults');
});

/* ── Getting a patch back into the C++ ───────────────────────────────────
 * The point of the rig: dial it in here, paste it into tools/panel_shot.cpp or
 * into the defaults in Parameters.cpp. */

function patchAsCpp() {
  const lines = ['/* Whoomp patch, from the web prototyper. */'];

  for (let i = 0; i < PARAMS.length; i++) {
    const p = PARAMS[i];
    const v = fromNorm(p.range, patch.knobs[i]);
    if (Math.abs(v - p.def) < 1e-4) continue;

    const num = Math.abs(v) >= 1000 ? v.toFixed(0) : v.toFixed(Math.abs(v) < 10 ? 3 : 2);
    lines.push(`setValue (proc, whm::pid::${cppId(p.id)}, ${num}f);`);
  }

  for (const [id, v] of Object.entries(patch.choices))
    if (v !== defaultPatch().choices[id])
      lines.push(`setNorm  (proc, whm::pid::${cppId(id)}, ${(v / 2).toFixed(1)}f);`);

  if (patch.toggles.limiter) lines.push('setNorm  (proc, whm::pid::limiter, 1.0f);');

  return lines.join('\n');
}

/** Panel ids to the pid:: names, which are arrays for the repeated sections. */
function cppId(id) {
  const m = {
    op1wave: 'opWave[0]', op2wave: 'opWave[1]',
    op1ratio: 'opRatio[0]', op2ratio: 'opRatio[1]',
    lvlsine: 'lvlSine', lvltri: 'lvlTri', lvlsaw: 'lvlSaw', lvlfold: 'lvlFold',
    foldenv: 'foldEnv', filtenv: 'filtEnv',
    suba: 'subA', subd: 'subD', subs: 'subS', subr: 'subR', sublevel: 'subLevel',
    fmindex: 'index', fmindexenv: 'indexEnv',
    fma: 'fmA', fmd: 'fmD', fms: 'fmS', fmr: 'fmR', fmlevel: 'fmLevel',
    cabmix: 'cabMix', roomsize: 'roomSize', roomdamp: 'roomDamp', roommix: 'roomMix',
    locut: 'loCut', hicut: 'hiCut', outlevel: 'outLevel',
  };
  if (m[id]) return m[id];

  const bell = id.match(/^bell([123])([fgq])$/);
  if (bell) {
    const kind = { f: 'bellFreq', g: 'bellGain', q: 'bellQ' }[bell[2]];
    return `${kind}[${+bell[1] - 1}]`;
  }
  return id;
}

const copy = (label, textToCopy) => {
  navigator.clipboard.writeText(textToCopy)
    .then(() => status(`${label} copied — ${textToCopy.split('\n').length} lines`))
    .catch(() => status('clipboard refused; check the console'));
  console.log(textToCopy);
};

document.getElementById('cpp').addEventListener('click', () => copy('C++ patch', patchAsCpp()));

document.getElementById('json').addEventListener('click', () => {
  const out = {};
  PARAMS.forEach((p, i) => { out[p.id] = +fromNorm(p.range, patch.knobs[i]).toFixed(4); });
  Object.assign(out, patch.choices, patch.toggles);
  copy('JSON patch', JSON.stringify(out, null, 2));
});

document.getElementById('wav').addEventListener('click', () => {
  if (!hit) { status('play one first — there is nothing rendered yet'); return; }
  const url = URL.createObjectURL(toWav(hit));
  const a = document.createElement('a');
  a.href = url;
  a.download = `whoomp-${L.noteName(play.note)}.wav`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  status('WAV saved');
});

/* ── Go ──────────────────────────────────────────────────────────────────── */

window.addEventListener('resize', resize);
resize();
syncControls();
drawScope();
requestAnimationFrame(frame);
status('space or the MIDI terminal plays one · drag a knob · shift for fine · double-click resets');
