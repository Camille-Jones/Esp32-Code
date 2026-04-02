/* app.js — SSE client for ESP32 dashboard */

const W = 400, H = 80, MAX_SAMPLES = 60;
const history = [];

// ── DOM refs ────────────────────────────────────────────────────────────────
const el = id => document.getElementById(id);

const dom = {
  vbatt1:   el('vbatt1'),
  vbatt2:   el('vbatt2'),
  vshunt:   el('vshunt'),
  pMotor:   el('p-motor'),
  pBrake:   el('p-brake'),
  pDMS:     el('p-dms'),
  powerPct: el('power-pct'),
  powerBar: el('power-bar'),
  watts:    el('watts'),
  sparkLine: el('spark-line'),
  sparkFill: el('spark-fill'),
  sparkAxes: el('spark-axes'),
  sparkYLabels: el('spark-y-labels'),
  connDot:  el('conn-dot'),
  connLabel:el('conn-label'),
  speedLines: el('speed-lines'),
  footer:   document.querySelector('.passion-quote'),
};

// ── Floating particles ──────────────────────────────────────────────────────
const PARTICLES = ['✦', '✧', '⋆', '★', '✶'];
function spawnParticles(n = 15) {
  for (let i = 0; i < n; i++) {
    const s = document.createElement('span');
    s.className = 'particle';
    s.textContent = PARTICLES[Math.floor(Math.random() * PARTICLES.length)];
    const size = 8 + Math.random() * 10;
    s.style.fontSize = size + 'px';
    s.style.setProperty('--x', Math.random() * 100 + '%');
    s.style.setProperty('--dur', (10 + Math.random() * 15) + 's');
    s.style.setProperty('--delay', -(Math.random() * 20) + 's');
    s.style.setProperty('--sway', (Math.random() * 60 - 30) + 'px');
    s.style.setProperty('--opa', (0.2 + Math.random() * 0.25).toFixed(2));
    document.body.appendChild(s);
  }
}
spawnParticles();

// ── Bouncy value updates ────────────────────────────────────────────────────
const prevValues = {};
function bounceIfChanged(id, newVal) {
  const span = el(id);
  if (!span) return;
  const parent = span.closest('.value');
  if (prevValues[id] !== undefined && prevValues[id] !== newVal && parent) {
    parent.classList.remove('bounce');
    void parent.offsetWidth; // reflow to restart animation
    parent.classList.add('bounce');
    parent.addEventListener('animationend', () => parent.classList.remove('bounce'), { once: true });
  }
  prevValues[id] = newVal;
}

// ── Footer quotes ───────────────────────────────────────────────────────────
const quotes = [
  '"website design is my passion" - Sam',
  '"it\'s not a bug, it\'s a feature"',
  '"the best error message is the one that never shows up"',
  '"speed has never killed anyone. suddenly becoming stationary, that\'s what gets you" — Jeremy Clarkson',
  '"measure with a micrometer, mark with chalk, cut with an axe"',
];
let quoteIdx = 0;
function rotateQuote() {
  dom.footer.style.opacity = '0';
  setTimeout(() => {
    quoteIdx = (quoteIdx + 1) % quotes.length;
    dom.footer.textContent = quotes[quoteIdx];
    dom.footer.style.opacity = '1';
  }, 400);
}
setInterval(rotateQuote, 8000);

// ── Max power acceleration ──────────────────────────────────────────────────
let maxPowerActive = false;
function checkMaxPower(pct) {
  const isMax = pct >= 100;
  if (isMax && !maxPowerActive) {
    maxPowerActive = true;
    document.body.classList.add('max-power');
    dom.speedLines.classList.add('active');
  } else if (!isMax && maxPowerActive) {
    maxPowerActive = false;
    document.body.classList.remove('max-power');
    dom.speedLines.classList.remove('active');
  }
}

// ── Helpers ─────────────────────────────────────────────────────────────────
function setPill(el, on) {
  el.classList.toggle('on', on);
  el.classList.toggle('off', !on);
}

function updateSparkline(watts) {
  history.push(watts);
  if (history.length > MAX_SAMPLES) history.shift();

  const max = Math.max(...history, 1);
  const n   = history.length;

  const pts = history.map((v, i) => {
    const x = Math.round(i * (W / (MAX_SAMPLES - 1)));
    const y = Math.round(H - 4 - (v / max) * (H - 12));
    return `${x},${y}`;
  }).join(' ');

  const lastX = Math.round((n - 1) * (W / (MAX_SAMPLES - 1)));
  const fill  = `${pts} ${lastX},${H - 1} 0,${H - 1}`;

  dom.sparkLine.setAttribute('points', pts);
  dom.sparkFill.setAttribute('points', fill);

  // Axes and grid lines (no text — labels are HTML)
  const ticks = [0, max / 2, max];
  let svg = '';
  svg += `<line x1="0" y1="4" x2="0" y2="${H - 4}" class="ax"/>`;
  svg += `<line x1="0" y1="${H - 4}" x2="${W}" y2="${H - 4}" class="ax"/>`;
  for (const val of ticks) {
    const y = Math.round(H - 4 - (val / max) * (H - 12));
    svg += `<line x1="0" y1="${y}" x2="${W}" y2="${y}" class="grid"/>`;
    svg += `<line x1="-4" y1="${y}" x2="0" y2="${y}" class="ax"/>`;
  }
  dom.sparkAxes.innerHTML = svg;

  // HTML Y-axis labels
  const topY = H - 4 - (H - 12);   // y for max value (top)
  const midY = H - 4 - (H - 12) / 2;
  const botY = H - 4;               // y for 0 (bottom)
  dom.sparkYLabels.innerHTML =
    `<span style="top:${(topY / H) * 100}%">${Math.round(max)}W</span>` +
    `<span style="top:${(midY / H) * 100}%">${Math.round(max / 2)}W</span>` +
    `<span style="top:${(botY / H) * 100}%">0W</span>`;
}

// ── Render a data payload ────────────────────────────────────────────────────
function render(d) {
  const v1 = parseFloat(d.Vbatt1).toFixed(2);
  const v2 = parseFloat(d.Vbatt2).toFixed(2);
  const vs = parseFloat(d.VShuntDC).toFixed(4);

  bounceIfChanged('vbatt1', v1);
  bounceIfChanged('vbatt2', v2);
  bounceIfChanged('vshunt', vs);
  dom.vbatt1.textContent = v1;
  dom.vbatt2.textContent = v2;
  dom.vshunt.textContent = vs;

  //console.log(vbatt1);

  setPill(dom.pMotor, d.MotorEnabled === true || d.MotorEnabled === 'true');
  setPill(dom.pBrake, d.BrakeEnabled === true || d.BrakeEnabled === 'true');
  setPill(dom.pDMS,   d.DMSEnabled   === true || d.DMSEnabled   === 'true');

  const pct = parseFloat(d.PowerPct);
  const pctStr = pct.toFixed(1);
  bounceIfChanged('power-pct', pctStr);
  dom.powerPct.textContent       = pctStr;
  dom.powerBar.style.width       = Math.min(Math.max(pct, 0), 100) + '%';

  const w = parseFloat(d.Watts);
  const wStr = w.toFixed(1);
  bounceIfChanged('watts', wStr);
  dom.watts.textContent = wStr;

  checkMaxPower(pct);
  updateSparkline(w);
}

// ── SSE connection ───────────────────────────────────────────────────────────
function connect() {
  const src = new EventSource('/events');

  src.addEventListener('data', e => {
    try {
      render(JSON.parse(e.data));
      dom.connDot.className   = 'status-dot live';
      dom.connLabel.textContent = 'Live';
    } catch (err) {
      console.error('Parse error:', err);
    }
  });

  src.onopen = () => {
    dom.connDot.className    = 'status-dot live';
    dom.connLabel.textContent = 'Live';
  };

  src.onerror = () => {
    dom.connDot.className    = 'status-dot';
    dom.connLabel.textContent = 'Reconnecting…';
    // EventSource reconnects automatically — no manual retry needed
  };
}

connect();

// ── Download logs ────────────────────────────────────────────────────────────
el('btn-download').addEventListener('click', () => {
  window.location.href = '/log';
});
