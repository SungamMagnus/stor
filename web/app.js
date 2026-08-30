/*
 * The prototyper: the panel wired to the mouse, the keyboard, MIDI, and the
 * offline renderer. Web-only affordances live in the strip under the panel so
 * the faceplate itself stays exactly what the plug-in draws.
 */

import * as P from './panel.js';
import { hue } from './panel.js';
import * as L from './layout.js';
import { PARAMS, INDEX, defaultPatch, gather, toNorm, fromNorm, valueOf,
         loadPatch, savePatch } from './params.js';
import { renderHit, metersAt, toWav } from './render.js';

const canvas = document.getElementById('panel');
const ctx = canvas.getContext('2d');
const scope = document.getElementById('scope');
const sctx = scope.getContext('2d');

/* A saved patch has to pass the same shape check loadPatch() always runs —
   see params.js — so anything left over from an old schema is discarded here
   rather than partially trusted. */
const restored = loadPatch();
const patch = restored ?? defaultPatch();

/** How the hit is played, as opposed to what it sounds like. */
const play = {
  note: 36, velocity: 1, gateMs: 100,
  auto: true,
  loop: false, bpm: 120, stepBeats: 1,
};

let audioCtx = null;
let hit = null;             // the last render: samples, meters, scope
let hitBuffer = null;       // the same thing, as an AudioBuffer
let stale = true;           // the patch has moved since that render

/* Every source started or scheduled, newest last. The meters read the most
   recent one that has actually begun, so a loop animates the hit you are
   hearing rather than the one that was rendered. */
let scheduled = [];
let nextBeat = 0;

const idle = { pitchEnv: 0, voiceEnv: 0, outLevel: 0, limitGr: 0, sounding: false };
let wasActive = false;
let dirty = true;

const statusEl = document.getElementById('status');
const status = s => { statusEl.textContent = s; };

/* ── Canvas sizing ───────────────────────────────────────────────────────── */

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.round(canvas.clientWidth * dpr);
  canvas.height = Math.round(canvas.clientHeight * dpr);
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

function noteHz() {
  const hz = valueOf(patch, INDEX.tune) * Math.pow(2, (play.note - 36) / 12);
  return hz >= 1000 ? (hz / 1000).toFixed(2) + ' kHz'
                    : (hz < 100 ? hz.toFixed(1) : Math.round(hz)) + ' Hz';
}

/** Meters for whichever hit is currently sounding. */
function liveMeters() {
  if (!audioCtx || !hit) return idle;

  const now = audioCtx.currentTime;
  let last = -Infinity;
  for (const s of scheduled) if (s.t <= now && s.t > last) last = s.t;
  if (last === -Infinity) return idle;

  const t = now - last;
  return t <= hit.seconds ? metersAt(hit, t) : idle;
}

function frame() {
  const m = liveMeters();
  const active = m.sounding || m.outLevel > 0.001;

  if (dirty || active || wasActive) {
    const dpr = canvas.width / P.designW;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    L.paint(ctx, patch, { ...m, noteText: `${L.noteName(play.note)}   ${noteHz()}` });
    dirty = false;
  }

  wasActive = active;
  requestAnimationFrame(frame);
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

  const n = hit.L.length, mid = h / 2, amp = h / 2 - 2;
  const k = 2.6, span = Math.pow(10, k) - 1;
  const at = p => Math.floor(n * (Math.pow(10, p * k) - 1) / span);

  sctx.fillStyle = 'rgba(79,126,168,0.85)';
  for (let px = 0; px < w; px++) {
    const a = at(px / w), b = at((px + 1) / w);
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

  /* Where the next hit lands, when there is going to be one. A tail crossing
     this line is a tail the loop is going to play over. */
  if (play.loop) {
    const beat = stepSeconds();
    if (beat < hit.seconds) {
      const px = w * Math.log10(1 + (beat / hit.seconds) * span) / k;
      sctx.strokeStyle = 'rgba(237,129,89,0.8)';
      sctx.setLineDash([3, 3]);
      sctx.beginPath(); sctx.moveTo(px, 0); sctx.lineTo(px, h); sctx.stroke();
      sctx.setLineDash([]);
    }
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

/** Re-render only when the patch has actually moved. A loop at 200 BPM asks
    for a hit three times a second, and re-rendering each one would be work
    done to produce exactly the same samples. */
function renderIfStale(report = true) {
  const ac = ensureAudio();
  if (!stale && hitBuffer) return;

  const t0 = performance.now();
  hit = renderHit(gather(patch), {
    note: play.note, velocity: play.velocity,
    gateMs: play.gateMs, sampleRate: ac.sampleRate,
  });
  const ms = performance.now() - t0;

  hitBuffer = ac.createBuffer(2, hit.L.length, hit.sampleRate);
  hitBuffer.copyToChannel(hit.L, 0);
  hitBuffer.copyToChannel(hit.R, 1);
  stale = false;

  drawScope();
  if (report) {
    status(`rendered ${hit.seconds.toFixed(2)} s in ${ms.toFixed(1)} ms · ` +
           `peak ${hit.peak.toFixed(3)}${hit.peak > 1 ? ' (over)' : ''} · ` +
           `latency ${hit.latency} samples`);
  }
}

/** One source, started at a time on the audio clock. */
function scheduleAt(t) {
  const src = audioCtx.createBufferSource();
  src.buffer = hitBuffer;
  src.connect(audioCtx.destination);
  src.start(t);
  scheduled.push({ t, src });

  /* Anything whose tail has finished is no longer anyone's business. */
  const now = audioCtx.currentTime;
  scheduled = scheduled.filter(s => s.t + hit.seconds > now - 0.1);
}

function trigger() {
  renderIfStale();
  scheduleAt(audioCtx.currentTime);

  /* Playing one by hand while the loop runs puts the loop back in step with
     it, so you can tap the panel into the groove rather than against it. */
  if (play.loop) nextBeat = audioCtx.currentTime + stepSeconds();
}

/* ── The loop ────────────────────────────────────────────────────────────
 * Sources are scheduled a little ahead on the audio clock rather than fired
 * from a timer, so the spacing is sample-accurate and a busy main thread
 * cannot push a hit late. Tails are left to overlap: at 160 BPM a 400 ms
 * decay runs into the next one, which is what a kick doing that sounds like.
 */

const LOOKAHEAD = 0.25;   // seconds of the future kept scheduled
const TICK = 40;          // milliseconds between wake-ups

const stepSeconds = () => 60 / play.bpm * play.stepBeats;

function scheduler() {
  if (!play.loop || !audioCtx || !hitBuffer) return;

  const step = stepSeconds();
  while (nextBeat < audioCtx.currentTime + LOOKAHEAD) {
    /* A wake-up that arrives very late must not fire a burst catching up. */
    if (nextBeat < audioCtx.currentTime - step) nextBeat = audioCtx.currentTime;
    scheduleAt(nextBeat);
    nextBeat += step;
  }
}

setInterval(scheduler, TICK);

function setLoop(on) {
  play.loop = on;

  if (on) {
    renderIfStale(false);
    nextBeat = audioCtx.currentTime + 0.06;
    scheduler();
    status(`looping at ${play.bpm} BPM · ${divLabel()} · ${stepSeconds().toFixed(3)} s a hit`);
  } else {
    /* Drop what has been scheduled but not yet sounded; leave what is already
       ringing to finish. */
    const now = audioCtx ? audioCtx.currentTime : 0;
    for (const s of scheduled)
      if (s.t > now) { try { s.src.stop(); } catch {} }
    scheduled = scheduled.filter(s => s.t <= now);
    status('loop stopped');
  }

  syncControls();
  drawScope();
}

/* ── Pointer ─────────────────────────────────────────────────────────────── */

let drag = null;

/** Any change to the sound invalidates the render, replays it if asked, and
    persists — so the tab you reload picks up where this one left off. */
function changed(replay = true) {
  stale = true;
  dirty = true;
  savePatch(patch);
  if (replay && play.auto && !play.loop) trigger();
  else if (replay && play.loop) renderIfStale(false);
}

canvas.addEventListener('pointerdown', e => {
  const d = toDesign(e);

  const radio = L.radioAt(d.x, d.y);
  if (radio) { patch.choices[radio.id] = radio.index; changed(); return; }

  if (L.limAt(d.x, d.y)) { patch.toggles.limiter = !patch.toggles.limiter; changed(); return; }
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

  patch.knobs[drag.i] = Math.max(0, Math.min(1,
    drag.startNorm + (drag.startY - d.y) / travel * sens));
  stale = true;
  dirty = true;
});

/* Auto: play again when a knob is let go. A kick is a quarter of a second, so
   the loop of move-something / hear-it wants to be one gesture, not two. */
const endDrag = () => {
  const was = drag;
  drag = null;
  if (was) changed();
};

canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);

canvas.addEventListener('dblclick', e => {
  const i = L.knobAt(toDesign(e).x, toDesign(e).y);
  if (i < 0) return;
  patch.knobs[i] = toNorm(PARAMS[i].range, PARAMS[i].def);
  changed();
});

canvas.addEventListener('wheel', e => {
  const d = toDesign(e);
  const i = L.knobAt(d.x, d.y);
  if (i < 0) return;
  e.preventDefault();
  const gain = e.shiftKey ? 0.04 : 0.16;
  patch.knobs[i] = Math.max(0, Math.min(1, patch.knobs[i] - Math.sign(e.deltaY) * gain));
  stale = true;
  dirty = true;
}, { passive: false });

/* ── Keyboard ────────────────────────────────────────────────────────────
 * Space plays the selected note; the bottom row walks up from C1, so the kick
 * and the toms above it are both one key away. */

const KEYS = 'zsxdcvgbhnjm';

window.addEventListener('keydown', e => {
  if (e.target !== document.body) return;

  if (e.code === 'Space') { e.preventDefault(); trigger(); return; }
  if (e.key.toLowerCase() === 'l' && !e.repeat) { ensureAudio(); setLoop(!play.loop); return; }

  const k = KEYS.indexOf(e.key.toLowerCase());
  if (k >= 0 && !e.repeat) { play.note = 36 + k; stale = true; syncControls(); trigger(); }
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
            play.velocity = d2 / 127;
            stale = true;
            syncControls();
            trigger();
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

const el = id => document.getElementById(id);
const noteEl = el('note'), velEl = el('vel'), velOut = el('velout');
const gateEl = el('gate'), gateOut = el('gateout');
const autoEl = el('auto'), loopEl = el('loop');
const bpmEl = el('bpm'), bpmOut = el('bpmout');

/** Beats per hit, and what to call it. */
const DIVISIONS = [
  { label: '1/4', beats: 1 },
  { label: '1/8', beats: 0.5 },
  { label: '1/8T', beats: 1 / 3 },
  { label: '1/16', beats: 0.25 },
];

const divLabel = () =>
  (DIVISIONS.find(d => Math.abs(d.beats - play.stepBeats) < 1e-6) || DIVISIONS[0]).label;

const divButtons = DIVISIONS.map(d => {
  const b = document.createElement('button');
  b.className = 'step div';
  b.textContent = d.label;
  b.addEventListener('click', () => {
    play.stepBeats = d.beats;
    syncControls();
    if (play.loop) status(`looping at ${play.bpm} BPM · ${d.label} · ` +
                          `${stepSeconds().toFixed(3)} s a hit`);
    drawScope();
  });
  el('div').appendChild(b);
  return { b, d };
});

function syncControls() {
  noteEl.textContent = L.noteName(play.note);
  velOut.textContent = Math.round(play.velocity * 127);
  velEl.value = String(Math.round(play.velocity * 127));
  gateOut.textContent = play.gateMs + ' ms';
  bpmOut.textContent = play.bpm + ' BPM';

  autoEl.setAttribute('aria-pressed', String(play.auto));
  autoEl.textContent = play.auto ? 'Auto on' : 'Auto off';
  loopEl.setAttribute('aria-pressed', String(play.loop));
  loopEl.textContent = play.loop ? 'Loop on' : 'Loop off';

  for (const { b, d } of divButtons)
    b.setAttribute('aria-pressed', String(Math.abs(d.beats - play.stepBeats) < 1e-6));

  dirty = true;
}

el('hit').addEventListener('click', () => trigger());
autoEl.addEventListener('click', () => { play.auto = !play.auto; syncControls(); });
loopEl.addEventListener('click', () => { ensureAudio(); setLoop(!play.loop); });

el('noteup').addEventListener('click', () => {
  play.note = Math.min(96, play.note + 1); stale = true; syncControls();
});
el('notedown').addEventListener('click', () => {
  play.note = Math.max(12, play.note - 1); stale = true; syncControls();
});

velEl.addEventListener('input', () => { play.velocity = +velEl.value / 127; stale = true; syncControls(); });
gateEl.addEventListener('input', () => { play.gateMs = +gateEl.value; stale = true; syncControls(); });

bpmEl.addEventListener('input', () => {
  play.bpm = +bpmEl.value;
  syncControls();
  drawScope();
  if (play.loop) status(`looping at ${play.bpm} BPM · ${divLabel()} · ` +
                        `${stepSeconds().toFixed(3)} s a hit`);
});

el('reset').addEventListener('click', () => {
  const fresh = defaultPatch();
  patch.knobs = fresh.knobs;
  patch.choices = fresh.choices;
  patch.toggles = fresh.toggles;
  changed(false);
  status('back to the plug-in defaults');
});

/* ── Getting a patch back into the C++ ───────────────────────────────────
 * The point of the rig: dial it in here, paste it into tools/panel_shot.cpp or
 * into the defaults in Parameters.cpp. */

function patchAsCpp() {
  const lines = ['/* Whoomp patch, from the web prototyper. */'];
  const defaults = defaultPatch();

  for (let i = 0; i < PARAMS.length; i++) {
    const p = PARAMS[i];
    const v = fromNorm(p.range, patch.knobs[i]);
    if (Math.abs(v - p.def) < 1e-4) continue;

    const num = Math.abs(v) >= 1000 ? v.toFixed(0) : v.toFixed(Math.abs(v) < 10 ? 3 : 2);
    lines.push(`setValue (proc, whm::pid::${cppId(p.id)}, ${num}f);`);
  }

  for (const [id, v] of Object.entries(patch.choices))
    if (v !== defaults.choices[id])
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

el('cpp').addEventListener('click', () => copy('C++ patch', patchAsCpp()));

el('json').addEventListener('click', () => {
  const out = {};
  PARAMS.forEach((p, i) => { out[p.id] = +fromNorm(p.range, patch.knobs[i]).toFixed(4); });
  Object.assign(out, patch.choices, patch.toggles);
  copy('JSON patch', JSON.stringify(out, null, 2));
});

el('wav').addEventListener('click', () => {
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

window.addEventListener('resize', () => { resize(); drawScope(); });
resize();
syncControls();
drawScope();
requestAnimationFrame(frame);
status((restored ? 'restored your last patch' : 'no saved patch — starting from the plug-in defaults') +
       ' · space or the MIDI terminal plays one · L loops · drag a knob · shift for fine · double-click resets');
