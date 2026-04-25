"""Generate a self-contained HTML profiling report."""

from __future__ import annotations

import html
import json
import re
from datetime import datetime
from typing import Optional

from .aggregator import FunctionReport, InstrStats, Report, SourceLineStats


def _heat_rgb(value: float, lo: float, hi: float) -> str:
    if hi <= lo:
        return "rgb(200,220,200)"
    t = max(0.0, min(1.0, (value - lo) / (hi - lo)))
    if t < 0.5:
        r = int(t * 2 * 220); g = 180
    else:
        r = 220; g = int((1 - (t - 0.5) * 2) * 180)
    return f"rgb({r},{g},0)"


def _efficiency_rgb(eff: float) -> str:
    t = max(0.0, min(1.0, eff))
    return f"rgb({int((1-t)*220)},{int(t*180)},0)"


def _fmt_pct(v: float) -> str:
    return f"{v * 100:.1f}%"


def _esc(s: str) -> str:
    return html.escape(str(s))


def _bar(value: float, max_value: float, width: int = 80, color: str = "#4a90d9") -> str:
    filled = 0 if max_value <= 0 else int(width * min(1.0, value / max_value))
    return (
        f'<svg width="{width}" height="12" style="vertical-align:middle">'
        f'<rect width="{width}" height="12" rx="2" fill="#e8e8e8"/>'
        f'<rect width="{filled}" height="12" rx="2" fill="{color}"/>'
        f"</svg>"
    )


def _kernel_id(fn: "FunctionReport") -> str:
    safe = re.sub(r"[^A-Za-z0-9_]", "_", fn.name)
    return f"k_l{fn.launch_id}_{safe}"


def _fmt_exec(n: int) -> str:
    if n >= 1_000_000:
        return f"{n / 1_000_000:.1f}M"
    if n >= 1_000:
        return f"{n / 1_000:.1f}K"
    return str(n)


_CSS = """
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
       background: #f0f2f5; color: #1a1a2e; font-size: 14px; }
header { background: #1a1a2e; color: #e8eaf6; padding: 20px 32px;
         display: flex; align-items: baseline; gap: 16px; }
header h1 { font-size: 20px; font-weight: 600; }
header .meta { font-size: 12px; opacity: .65; }
.container { max-width: 1600px; margin: 0 auto; padding: 24px 32px; }
section.card { background: #fff; border-radius: 10px; padding: 20px 24px;
               margin-bottom: 20px; box-shadow: 0 1px 4px rgba(0,0,0,.10); }
h2 { font-size: 15px; font-weight: 600; margin-bottom: 14px; color: #1a1a2e; }
h3 { font-size: 13px; font-weight: 600; margin: 16px 0 8px; color: #444; }

/* Summary table */
.summary-table { width: 100%; border-collapse: collapse; }
.summary-table th, .summary-table td { padding: 8px 12px; text-align: left;
    border-bottom: 1px solid #eee; white-space: nowrap; }
.summary-table th { font-size: 11px; text-transform: uppercase;
    letter-spacing: .06em; color: #666; background: #fafafa; }
.summary-table tbody tr:hover { background: #f5f7ff; cursor: pointer; }
.summary-table .name-cell { font-family: monospace; font-size: 12px;
    color: #1a1a2e; max-width: 360px; overflow: hidden; text-overflow: ellipsis; }

/* Kernel section */
.kernel-section { display: none; }
.kernel-section.active { display: block; }
.kernel-header { display: flex; align-items: center; gap: 12px;
                 margin-bottom: 16px; flex-wrap: wrap; }
.kernel-title { font-size: 14px; font-weight: 600; font-family: monospace;
                word-break: break-all; }
.badge { display: inline-flex; align-items: center; gap: 4px;
         padding: 2px 8px; border-radius: 12px; font-size: 11px;
         font-weight: 600; background: #e8f4fd; color: #1565c0; }
.badge.warn { background: #fff3e0; color: #e65100; }
.badge.danger { background: #fdecea; color: #b71c1c; }
.back-btn { padding: 4px 12px; font-size: 12px; border: 1px solid #ddd;
             border-radius: 6px; background: #fff; cursor: pointer; color: #444; }
.back-btn:hover { background: #f5f5f5; }

/* Tabs */
.tabs { display: flex; gap: 2px; margin-bottom: 16px; border-bottom: 2px solid #e0e0e0; }
.tab-btn { padding: 6px 16px; font-size: 13px; border: none; background: none;
           cursor: pointer; color: #666; border-bottom: 2px solid transparent;
           margin-bottom: -2px; border-radius: 4px 4px 0 0; transition: all .15s; }
.tab-btn:hover { background: #f5f5f5; color: #333; }
.tab-btn.active { color: #1a1a2e; font-weight: 600;
                  border-bottom-color: #3f51b5; background: #f0f2ff; }
.tab-pane { display: none; }
.tab-pane.active { display: block; }

/* =========================================================================
   SOURCE ↔ ASSEMBLY SPLIT VIEW  (AgGrid-based)
   ========================================================================= */

.sa-hint {
    font-size: 11px; color: #888; margin-bottom: 8px;
    padding: 5px 10px; background: #f8f9fa; border-radius: 4px;
    border-left: 3px solid #b0bec5;
}

.sa-split {
    display: flex;
    height: calc(100vh - 220px); min-height: 500px; max-height: 1200px;
    border: 1px solid #dde0e6; border-radius: 8px; overflow: hidden;
}

.sa-left  { flex: 0 0 42%; display: flex; flex-direction: column; min-width: 120px; }
.sa-right { flex: 1 1 0;   display: flex; flex-direction: column; min-width: 120px; overflow: hidden; }

/* Drag divider */
.sa-divider {
    flex: 0 0 5px; background: #dde0e6; cursor: col-resize;
    transition: background .15s; flex-shrink: 0; z-index: 10;
    display: flex; align-items: center; justify-content: center;
}
.sa-divider::after {
    content: ''; width: 1px; height: 40px;
    background: #b0bec5; border-radius: 1px;
}
.sa-divider:hover, .sa-divider.sa-dragging { background: #9fa8da; }
.sa-divider.sa-dragging::after { background: #3f51b5; width: 2px; }

/* Panel headers */
.sa-panel-hdr {
    background: #2a2e3d; color: #c8d0e7;
    padding: 6px 14px; font-size: 10px; font-weight: 700;
    text-transform: uppercase; letter-spacing: .08em;
    display: flex; justify-content: space-between; align-items: center;
    flex-shrink: 0;
}

/* AgGrid container fills the panel body */
.sa-ag-grid { flex: 1; min-height: 0; }

/* ---- AgGrid Alpine theme overrides ---- */
.ag-theme-alpine {
    --ag-font-family: 'Consolas','Monaco',monospace;
    --ag-font-size: 12px;
    --ag-grid-size: 3px;
    --ag-row-height: 22px;
    --ag-header-height: 28px;
    --ag-border-color: #dde0e6;
    --ag-row-border-color: #f0f0f0;
    --ag-odd-row-background-color: #fff;
    --ag-row-hover-color: transparent;
    --ag-selected-row-background-color: transparent;
    --ag-header-background-color: #f5f6fa;
    --ag-header-foreground-color: #555;
    --ag-header-column-resize-handle-color: #c5cae9;
    --ag-cell-horizontal-padding: 8px;
    --ag-borders: solid 1px;
    --ag-border-color: #dde0e6;
    --ag-wrapper-border-radius: 0;
    --ag-side-bar-panel-width: 0;
}
.ag-theme-alpine .ag-header-cell-label { font-weight: 700; letter-spacing: .05em; }
.ag-theme-alpine .ag-row { border-bottom: 1px solid #f5f5f5; }
/* Row highlight states */
.ag-theme-alpine .ag-row.sa-hl    { background: #fff8c5 !important; }
.ag-theme-alpine .ag-row.sa-pinned { background: #fff0a0 !important;
    box-shadow: inset 2px 0 0 #f9a825; }
/* Source-panel specific */
.ag-theme-alpine .ag-row.sa-cold { color: #bbb; }
/* Separator rows in assembly panel */
.ag-theme-alpine .ag-row.sa-sep {
    background: #f0f2f8 !important;
    border-top: 1px solid #e0e4ef !important;
}
/* Left panel: source */
#sa-src-panel .ag-theme-alpine { --ag-font-family: 'Consolas','Monaco',monospace; }

/* =========================================================================
   Assembly-only table (fallback / "Assembly" tab)
   ========================================================================= */
.asm-table { width: 100%; border-collapse: collapse; font-size: 12px; }
.asm-table th { font-size: 10px; text-transform: uppercase; letter-spacing: .06em;
    color: #666; background: #fafafa; padding: 6px 8px; text-align: left;
    border-bottom: 2px solid #e0e0e0; white-space: nowrap; }
.asm-table td { padding: 4px 8px; border-bottom: 1px solid #f0f0f0;
    vertical-align: middle; white-space: nowrap; }
.asm-table tbody tr:hover td { background: #f8f9ff; }
.asm-table .pc-cell { font-family: monospace; color: #777; font-size: 11px; }
.asm-table .bb-cell { font-family: monospace; font-size: 11px; color: #999;
    max-width: 120px; overflow: hidden; text-overflow: ellipsis; }
.asm-table .instr-cell { font-family: monospace; color: #2c3e50; font-size: 12px;
    max-width: 340px; overflow: hidden; text-overflow: ellipsis; }
.asm-table .metric-cell { text-align: right; font-variant-numeric: tabular-nums; }
.asm-table .metric-bar-cell { min-width: 120px; }
.asm-table tr.hot-row td { background: #fff8f0; }

/* Hotspot / source tables */
.src-table { width: 100%; border-collapse: collapse; font-size: 12px; }
.src-table th { font-size: 10px; text-transform: uppercase; letter-spacing: .06em;
    color: #666; background: #fafafa; padding: 6px 8px; text-align: left;
    border-bottom: 2px solid #e0e0e0; }
.src-table td { padding: 4px 8px; border-bottom: 1px solid #f0f0f0; vertical-align: middle; }
.src-table .lineno { color: #aaa; font-family: monospace; text-align: right;
    padding-right: 12px; user-select: none; white-space: nowrap; }
.src-table .code { font-family: monospace; white-space: pre;
    max-width: 600px; overflow: hidden; text-overflow: ellipsis; }
.src-table .metric-cell { text-align: right; white-space: nowrap; }
.hot-table { width: 100%; border-collapse: collapse; font-size: 12px; }
.hot-table th { font-size: 10px; text-transform: uppercase; letter-spacing: .06em;
    color: #666; background: #fafafa; padding: 6px 8px; text-align: left;
    border-bottom: 2px solid #e0e0e0; }
.hot-table td { padding: 5px 8px; border-bottom: 1px solid #f0f0f0;
    vertical-align: middle; white-space: nowrap; }

/* Shared widgets */
.bar-wrap { display: inline-flex; align-items: center; gap: 6px; }
.bar-wrap .val { font-variant-numeric: tabular-nums; min-width: 54px;
    text-align: right; color: #333; }
.eff-pct { font-variant-numeric: tabular-nums; display: inline-block;
    min-width: 42px; text-align: right; }
.stat-grid { display: flex; gap: 16px; flex-wrap: wrap; margin-bottom: 16px; }
.stat-pill { background: #f0f2f5; border-radius: 8px; padding: 10px 16px; min-width: 140px; }
.stat-pill .label { font-size: 10px; text-transform: uppercase;
    letter-spacing: .06em; color: #777; margin-bottom: 4px; }
.stat-pill .value { font-size: 18px; font-weight: 700; color: #1a1a2e;
    font-variant-numeric: tabular-nums; }
.stat-pill .sub { font-size: 11px; color: #999; margin-top: 2px; }
.itype-chart { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 8px; }
.itype-bar { display: flex; flex-direction: column; align-items: center;
    font-size: 10px; color: #555; }
.itype-bar .bar-body { width: 24px; background: #c5cae9; border-radius: 2px 2px 0 0;
    margin-top: 2px; min-height: 2px; }
.itype-bar .bar-label { margin-top: 4px; }
.empty { color: #999; font-style: italic; padding: 16px 0; }
.table-scroll { overflow-x: auto; }

@media (max-width: 900px) {
    .sa-split { flex-direction: column; height: auto; }
    .sa-left  { flex: none; height: 55vh; }
    .sa-divider { flex: 0 0 5px; }
    .sa-right { flex: none; height: 55vh; }
    .container { padding: 12px 16px; }
}
"""

_JS = r"""
// =========================================================================
// Navigation
// =========================================================================

const _gridInited = new Set();

function showKernel(id) {
    document.querySelectorAll('.kernel-section').forEach(s => s.classList.remove('active'));
    document.getElementById(id).classList.add('active');
    document.getElementById('summary-section').style.display = 'none';
    window.scrollTo(0, 0);
    // Lazy-init AgGrid for this kernel the first time it's shown
    if (!_gridInited.has(id)) {
        _gridInited.add(id);
        setTimeout(() => {
            const sa = document.querySelector('#' + id + ' .sa-split');
            if (sa && sa.dataset.kid) _saInitGrids(sa.dataset.kid);
        }, 0);
    }
}
function showSummary() {
    document.querySelectorAll('.kernel-section').forEach(s => s.classList.remove('active'));
    document.getElementById('summary-section').style.display = '';
}
function switchTab(kernelId, tab) {
    document.querySelectorAll('#' + kernelId + ' .tab-btn')
        .forEach(b => b.classList.remove('active'));
    document.querySelectorAll('#' + kernelId + ' .tab-pane')
        .forEach(p => p.classList.remove('active'));
    document.querySelector('#' + kernelId + ' .tab-btn[data-tab="' + tab + '"]').classList.add('active');
    document.getElementById(kernelId + '-tab-' + tab).classList.add('active');
}

// =========================================================================
// Split-pane drag-resize
// =========================================================================

function _initPaneResize(splitEl) {
    const div  = splitEl.querySelector('.sa-divider');
    const left = splitEl.querySelector('.sa-left');
    if (!div || !left) return;
    let x0, w0;
    div.addEventListener('mousedown', e => {
        e.preventDefault();
        x0 = e.clientX;
        w0 = left.getBoundingClientRect().width;
        div.classList.add('sa-dragging');
        document.body.style.cursor = 'col-resize';
        document.body.style.userSelect = 'none';
        function mv(e) {
            const max = splitEl.getBoundingClientRect().width - 5;
            left.style.flex = '0 0 ' + Math.max(120, Math.min(max - 120, w0 + e.clientX - x0)) + 'px';
            // Tell AgGrid to re-fit after resize
            splitEl.querySelectorAll('.sa-ag-grid').forEach(g => {
                if (g._gridApi) g._gridApi.sizeColumnsToFit();
            });
        }
        function up() {
            div.classList.remove('sa-dragging');
            document.body.style.cursor = document.body.style.userSelect = '';
            document.removeEventListener('mousemove', mv);
            document.removeEventListener('mouseup', up);
        }
        document.addEventListener('mousemove', mv);
        document.addEventListener('mouseup', up);
    });
}

// =========================================================================
// Client-side colour helpers (mirror Python)
// =========================================================================

function _heatRgb(v, lo, hi) {
    if (hi <= lo) return 'rgb(200,220,200)';
    const t = Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
    const r = t < .5 ? Math.round(t * 2 * 220) : 220;
    const g = t < .5 ? 180 : Math.round((1 - (t - .5) * 2) * 180);
    return `rgb(${r},${g},0)`;
}
function _effRgb(e) {
    const t = Math.max(0, Math.min(1, e));
    return `rgb(${Math.round((1-t)*220)},${Math.round(t*180)},0)`;
}
function _fmtN(n) {
    if (n >= 1e6) return (n/1e6).toFixed(1) + 'M';
    if (n >= 1e3) return (n/1e3).toFixed(1) + 'K';
    return String(n);
}
function _miniBar(pct, color, valStr, valColor) {
    return `<div style="display:flex;align-items:center;gap:5px">` +
        `<div style="flex:0 0 54px;height:9px;background:#e8e8e8;border-radius:2px;overflow:hidden">` +
        `<div style="width:${pct}%;height:100%;background:${color};border-radius:2px"></div></div>` +
        `<span style="font-size:11px;font-variant-numeric:tabular-nums;color:${valColor||'#333'}">${valStr}</span></div>`;
}

// =========================================================================
// AgGrid source ↔ assembly split view
// =========================================================================

function _saInitGrids(kid) {
    const kd = (window.KERNEL_DATA || {})[kid];
    if (!kd) return;

    const srcEl = document.getElementById(kid + '-src-grid');
    const asmEl = document.getElementById(kid + '-asm-grid');
    if (!srcEl || !asmEl) return;

    let pinned = null;
    let srcApi, asmApi;

    // ---- Highlight helpers ----
    function _setHl(api, sid, cls) {
        const changed = [];
        api.forEachNode(n => {
            const want = (sid && n.data.sid === sid) ? cls : null;
            if (n.data._hl !== want) { n.data._hl = want; changed.push(n); }
        });
        if (changed.length) api.redrawRows({ rowNodes: changed });
    }
    function _doHl(sid, cls) { _setHl(srcApi, sid, cls); _setHl(asmApi, sid, cls); }
    function _clearHl() { _doHl(null, null); }

    function _scrollTo(api, sid) {
        let idx = -1;
        api.forEachNode(n => { if (idx < 0 && n.data.sid === sid) idx = n.rowIndex; });
        if (idx >= 0) api.ensureIndexVisible(idx, 'middle');
    }

    // ---- Column definitions ----
    const srcCols = [
        {
            field: 'ln', headerName: 'Line', width: 54, minWidth: 40,
            resizable: true,
            cellStyle: p => {
                if (!p.data.active) return { color: '#ccc', borderLeft: '3px solid transparent' };
                return {
                    fontWeight: 600, color: '#444',
                    borderLeft: '3px solid ' + _heatRgb(p.data.exec, 0, kd.maxSrcExec),
                };
            },
        },
        {
            field: 'text', headerName: 'Source', flex: 1, minWidth: 80,
            resizable: true,
            cellStyle: p => ({
                color: p.data.active ? '#1e272e' : '#bbb',
                whiteSpace: 'pre', overflow: 'hidden',
            }),
        },
        {
            field: 'n_ptx', headerName: 'PTX', width: 50, minWidth: 36,
            resizable: true,
            cellRenderer: p => {
                if (!p.value) return '';
                return `<span style="display:inline-block;background:#e8eaf6;color:#3f51b5;` +
                    `border-radius:9px;padding:0 5px;font-size:9px;font-weight:700">${p.value}</span>`;
            },
        },
        {
            field: 'exec', headerName: 'Exec', width: 130, minWidth: 80,
            resizable: true,
            cellRenderer: p => {
                if (!p.data.active) return '';
                const pct = Math.round(100 * p.value / kd.maxSrcExec);
                return _miniBar(pct, _heatRgb(p.value, 0, kd.maxSrcExec), _fmtN(p.value));
            },
        },
        {
            field: 'eff', headerName: 'Branch Eff', width: 120, minWidth: 70,
            resizable: true,
            cellRenderer: p => {
                if (!p.data.active) return '';
                const c = _effRgb(p.value);
                return _miniBar(Math.round(p.value * 100), c,
                    (p.value * 100).toFixed(1) + '%', c);
            },
        },
    ];

    const asmCols = [
        {
            field: 'pc', headerName: 'PC', width: 72, minWidth: 50,
            resizable: true,
            cellStyle: { color: '#9aa3b5', fontSize: '11px' },
            cellRenderer: p => p.data.is_sep
                ? `<span style="color:#7986cb;font-weight:700;letter-spacing:.04em">${p.data.sep_label}</span>`
                : p.value,
        },
        {
            field: 'ptx', headerName: 'PTX Instruction', flex: 1, minWidth: 100,
            resizable: true,
            cellStyle: { color: '#1e272e', whiteSpace: 'pre', overflow: 'hidden' },
            cellRenderer: p => p.data.is_sep ? '' : p.value,
            tooltipField: 'ptx',
        },
        {
            field: 'exec', headerName: 'Exec Count', width: 150, minWidth: 80,
            resizable: true,
            cellRenderer: p => {
                if (p.data.is_sep) return '';
                return _miniBar(Math.round(100 * p.value / kd.maxExec),
                    _heatRgb(p.value, 0, kd.maxExec), _fmtN(p.value));
            },
        },
        {
            field: 'eff', headerName: 'Branch Eff', width: 130, minWidth: 70,
            resizable: true,
            cellRenderer: p => {
                if (p.data.is_sep) return '';
                const c = _effRgb(p.value);
                return _miniBar(Math.round(p.value * 100), c,
                    (p.value * 100).toFixed(1) + '%', c);
            },
        },
    ];
    if (kd.hasBc) {
        asmCols.push({
            field: 'bc', headerName: 'Bank Conflicts', width: 110, minWidth: 60,
            resizable: true,
            cellRenderer: p => {
                if (p.data.is_sep || !p.value) return p.data.is_sep ? '' : '0';
                return `<span style="color:#d32f2f;font-weight:600">${p.value.toLocaleString()}</span>`;
            },
        });
    }

    // ---- Common grid options ----
    const common = {
        defaultColDef: { sortable: false, suppressMovable: true, resizable: true },
        suppressCellFocus: true,
        suppressRowClickSelection: true,
        enableCellTextSelection: true,
        tooltipShowDelay: 600,
        rowClassRules: {
            'sa-hl':     p => p.data._hl === 'hl',
            'sa-pinned': p => p.data._hl === 'pinned',
            'sa-cold':   p => !p.data.active,
            'sa-sep':    p => !!p.data.is_sep,
        },
    };

    // ---- Source grid ----
    srcApi = agGrid.createGrid(srcEl, {
        ...common,
        columnDefs: srcCols,
        rowData: kd.src,
        onCellMouseOver: e => {
            if (pinned !== null || !e.data.sid) return;
            _doHl(e.data.sid, 'hl');
            _scrollTo(asmApi, e.data.sid);
        },
        onCellMouseLeave: () => { if (pinned === null) _clearHl(); },
        onRowClicked: e => {
            if (!e.data.sid) return;
            if (e.data.sid === pinned) { pinned = null; _clearHl(); }
            else { pinned = e.data.sid; _doHl(e.data.sid, 'pinned'); _scrollTo(asmApi, e.data.sid); }
        },
    });

    // ---- Assembly grid ----
    asmApi = agGrid.createGrid(asmEl, {
        ...common,
        columnDefs: asmCols,
        rowData: kd.asm,
        onCellMouseOver: e => {
            if (pinned !== null || !e.data.sid || e.data.is_sep) return;
            _doHl(e.data.sid, 'hl');
            _scrollTo(srcApi, e.data.sid);
        },
        onCellMouseLeave: () => { if (pinned === null) _clearHl(); },
        onRowClicked: e => {
            if (!e.data.sid || e.data.is_sep) return;
            if (e.data.sid === pinned) { pinned = null; _clearHl(); }
            else { pinned = e.data.sid; _doHl(e.data.sid, 'pinned'); _scrollTo(srcApi, e.data.sid); }
        },
    });

    // Store references for pane resize
    srcEl._gridApi = srcApi;
    asmEl._gridApi = asmApi;

    // Init pane divider drag
    const splitEl = document.querySelector('.sa-split[data-kid="' + kid + '"]');
    if (splitEl) _initPaneResize(splitEl);
}
"""

_AGGRID_CDN = """\
<link  rel="stylesheet" href="https://cdn.jsdelivr.net/npm/ag-grid-community@32.3.3/styles/ag-grid.css">
<link  rel="stylesheet" href="https://cdn.jsdelivr.net/npm/ag-grid-community@32.3.3/styles/ag-theme-alpine.css">
<script src="https://cdn.jsdelivr.net/npm/ag-grid-community@32.3.3/dist/ag-grid-community.min.js"></script>"""


def _render_split_view(fn: FunctionReport, source_files: dict[str, list[str]]) -> str:
    kid = _kernel_id(fn)
    all_files = sorted(fn.source_lines.keys())
    file_idx = {f: i for i, f in enumerate(all_files)}

    def _sid(sf: str, ln: int) -> str:
        return f"f{file_idx[sf]}l{ln}"

    max_exec = max((i.issue_count for i in fn.instructions), default=1)

    ptx_counts: dict[tuple[str, int], int] = {}
    for instr in fn.instructions:
        if instr.source_file and instr.source_line is not None:
            key = (instr.source_file, instr.source_line)
            ptx_counts[key] = ptx_counts.get(key, 0) + 1

    max_src_exec = max(
        (s.exec_count
         for line_map in fn.source_lines.values()
         for s in line_map.values()),
        default=1,
    )

    src_rows: list[dict] = []
    row_id = 0
    for sf in all_files:
        line_map = fn.source_lines[sf]
        full_src: list[str] = source_files.get(sf, [])

        if full_src:
            max_ln = max(max(line_map.keys(), default=1), len(full_src) - 1)
            line_range: list[int] = list(range(1, max_ln + 1))
        else:
            shown: set[int] = set()
            for ln in line_map:
                for l in range(max(1, ln - 3), ln + 4):
                    shown.add(l)
            line_range = sorted(shown)

        for ln in line_range:
            sl = line_map.get(ln)
            is_active = sl is not None
            n_ptx = ptx_counts.get((sf, ln), 0)

            if sl and sl.source_text:
                text = sl.source_text
            elif full_src and 0 < ln < len(full_src):
                text = full_src[ln]
            else:
                text = ""

            src_rows.append({
                "_id":    row_id,
                "ln":     ln,
                "text":   text,
                "active": is_active,
                "sid":    _sid(sf, ln) if n_ptx > 0 else None,
                "n_ptx":  n_ptx,
                "exec":   sl.exec_count if is_active else 0,
                "eff":    sl.avg_branch_efficiency if is_active else 1.0,
                "bc":     sl.bank_conflicts_total if is_active else 0,
                "_hl":    None,
            })
            row_id += 1

    asm_rows: list[dict] = []
    prev_src_key: Optional[tuple] = None
    row_id = 0

    for instr in fn.instructions:
        sid = ""
        if instr.source_file and instr.source_line is not None:
            sf = instr.source_file
            ln = instr.source_line
            sid = _sid(sf, ln)
            src_key = (sf, ln)
            if src_key != prev_src_key:
                asm_rows.append({
                    "_id":       f"sep{row_id}",
                    "is_sep":    True,
                    "sep_label": f"{sf.split('/')[-1]}:{ln}",
                    "pc": "", "ptx": "", "exec": 0, "eff": 1.0, "bc": 0,
                    "sid": None, "active": False, "_hl": None,
                })
                prev_src_key = src_key

        asm_rows.append({
            "_id":    row_id,
            "is_sep": False,
            "pc":     f"0x{instr.pc:04x}",
            "ptx":    instr.ptx_line or instr.instr_name,
            "exec":   instr.issue_count,
            "eff":    instr.avg_branch_efficiency,
            "bc":     instr.bank_conflicts_total,
            "sid":    sid,
            "active": True,
            "_hl":    None,
        })
        row_id += 1

    kd = {
        "src":        src_rows,
        "asm":        asm_rows,
        "hasBc":      fn.has_bank_conflicts,
        "maxExec":    max_exec,
        "maxSrcExec": max_src_exec,
    }
    data_js = (
        f'<script>window.KERNEL_DATA=window.KERNEL_DATA||{{}};'
        f'window.KERNEL_DATA["{kid}"]={json.dumps(kd, ensure_ascii=False, separators=(",",":"))};</script>'
    )


    src_panel = (
        f'<div class="sa-panel-hdr">Source</div>'
        f'<div id="{kid}-src-grid" class="ag-theme-alpine sa-ag-grid"></div>'
    )
    asm_panel = (
        f'<div class="sa-panel-hdr">PTX Assembly</div>'
        f'<div id="{kid}-asm-grid" class="ag-theme-alpine sa-ag-grid"></div>'
    )

    return (
        '<p class="sa-hint">'
        'Hover to highlight correspondence &mdash; click to pin. '
        'Drag the centre bar to resize panels; drag column edges to resize columns.'
        '</p>'
        f'<div class="sa-split" data-kid="{kid}">'
        f'<div class="sa-left"  id="{kid}-src-panel">{src_panel}</div>'
        f'<div class="sa-divider" title="Drag to resize"></div>'
        f'<div class="sa-right" id="{kid}-asm-panel">{asm_panel}</div>'
        f'</div>'
        f'{data_js}'
    )






def _render_asm_table(fn: FunctionReport) -> str:
    max_exec = max((i.issue_count for i in fn.instructions), default=1)
    has_bc = fn.has_bank_conflicts

    cols = ["PC", "Basic Block", "PTX Instruction", "Exec Count", "Branch Eff"]
    if has_bc:
        cols.append("Bank Conflicts")

    thead = "".join(f"<th>{c}</th>" for c in cols)
    rows = []

    for instr in fn.instructions:
        eff = instr.avg_branch_efficiency
        hot = instr.issue_count >= max_exec * 0.8
        rc = ' class="hot-row"' if hot else ""

        exec_bar = _bar(instr.issue_count, max_exec, 70, _heat_rgb(instr.issue_count, 0, max_exec))
        eff_bar  = _bar(eff, 1.0, 60, _efficiency_rgb(eff))
        ptx_line = _esc(instr.ptx_line or instr.instr_name)

        cells = [
            f'<td class="pc-cell">0x{instr.pc:04x}</td>',
            f'<td class="bb-cell" title="{_esc(instr.basic_block)}">{_esc(instr.basic_block)}</td>',
            f'<td class="instr-cell" title="{ptx_line}">{ptx_line}</td>',
            f'<td class="metric-bar-cell"><span class="bar-wrap">{exec_bar}'
            f'<span class="val">{_fmt_exec(instr.issue_count)}</span></span></td>',
            f'<td class="metric-bar-cell"><span class="bar-wrap">{eff_bar}'
            f'<span class="eff-pct">{_fmt_pct(eff)}</span></span></td>',
        ]
        if has_bc:
            bc = instr.bank_conflicts_total
            bc_c = "#d32f2f" if bc > 0 else "#388e3c"
            cells.append(f'<td class="metric-cell" style="color:{bc_c}">{bc:,}</td>')

        rows.append(f"<tr{rc}>{''.join(cells)}</tr>")

    return (
        '<div class="table-scroll">'
        f'<table class="asm-table"><thead><tr>{thead}</tr></thead>'
        f'<tbody>{"".join(rows)}</tbody></table></div>'
    )






def _render_hotspots(fn: FunctionReport) -> str:
    def _hot_table(title: str, instrs: list[InstrStats]) -> str:
        if not instrs:
            return ""
        cols = ["PC", "PTX Instruction", "Exec Count", "Branch Eff", "Bank Conflicts"]
        if fn.has_source:
            cols.insert(2, "Src")
        thead = "".join(f"<th>{c}</th>" for c in cols)
        max_exec = max(i.issue_count for i in fn.instructions) or 1
        rows = []
        for instr in instrs:
            eff = instr.avg_branch_efficiency
            bc = instr.bank_conflicts_total
            cells = [
                f'<td class="pc-cell">0x{instr.pc:04x}</td>',
                f'<td class="instr-cell" title="{_esc(instr.ptx_line or instr.instr_name)}">'
                f'{_esc(instr.ptx_line or instr.instr_name)}</td>',
            ]
            if fn.has_source:
                cells.insert(1, f'<td>{instr.source_line or ""}</td>')
            cells += [
                f'<td><span class="bar-wrap">'
                f'{_bar(instr.issue_count, max_exec, 60, _heat_rgb(instr.issue_count, 0, max_exec))}'
                f'<span class="val">{_fmt_exec(instr.issue_count)}</span></span></td>',
                f'<td><span class="bar-wrap">'
                f'{_bar(eff, 1.0, 50, _efficiency_rgb(eff))}'
                f'<span class="eff-pct">{_fmt_pct(eff)}</span></span></td>',
                f'<td style="color:{"#d32f2f" if bc > 0 else "#388e3c"}">{bc:,}</td>',
            ]
            rows.append(f"<tr>{''.join(cells)}</tr>")
        return (
            f"<h3>{title}</h3>"
            '<div class="table-scroll">'
            f'<table class="hot-table"><thead><tr>{thead}</tr></thead>'
            f'<tbody>{"".join(rows)}</tbody></table></div>'
        )

    sections = [_hot_table("Top instructions by execution count", fn.hotspots_by_exec(10))]
    div_hot = fn.hotspots_by_divergence(10)
    if div_hot:
        sections.append(_hot_table("Most divergent (lowest branch efficiency)", div_hot))
    bc_hot = fn.hotspots_by_conflicts(10)
    if bc_hot:
        sections.append(_hot_table("Instructions with most bank conflicts", bc_hot))

    return "\n".join(s for s in sections if s) or '<p class="empty">No hotspot data.</p>'






def _render_itype_chart(fn: FunctionReport) -> str:
    counts: dict[str, int] = {}
    for i in fn.instructions:
        counts[i.instr_name] = counts.get(i.instr_name, 0) + i.issue_count
    if not counts:
        return ""
    max_count = max(counts.values())
    palette = ["#3f51b5","#e91e63","#009688","#ff5722","#607d8b",
               "#9c27b0","#ff9800","#2196f3","#4caf50","#795548"]
    bars = []
    for idx, (name, cnt) in enumerate(sorted(counts.items(), key=lambda x: -x[1])[:16]):
        h = max(2, int(60 * cnt / max_count))
        color = palette[idx % len(palette)]
        bars.append(
            f'<div class="itype-bar" title="{_esc(name)}: {_fmt_exec(cnt)}">'
            f'<div class="bar-body" style="height:{h}px;background:{color}"></div>'
            f'<div class="bar-label">{_esc(name)}</div></div>'
        )
    return f'<div class="itype-chart">{"".join(bars)}</div>'






def _render_stat_pills(fn: FunctionReport) -> str:
    eff = fn.avg_branch_efficiency
    pills = [
        ("Total Executions", _fmt_exec(fn.total_exec_count), "warp-instruction issues"),
        ("Avg Branch Eff",   f"{eff*100:.1f}%", "higher is better (1.0 = fully converged)"),
        ("Bank Conflicts",   f"{fn.total_bank_conflicts:,}", "shared memory bank conflicts"),
        ("Unique PCs",       str(len(fn.instructions)), "distinct instructions profiled"),
    ]
    parts = []
    for label, value, sub in pills:
        parts.append(
            f'<div class="stat-pill">'
            f'<div class="label">{_esc(label)}</div>'
            f'<div class="value">{_esc(value)}</div>'
            f'<div class="sub">{_esc(sub)}</div>'
            f'</div>'
        )
    return f'<div class="stat-grid">{"".join(parts)}</div>'






def _render_kernel_section(fn: FunctionReport, source_files: dict[str, list[str]]) -> str:
    kid = _kernel_id(fn)
    has_src = fn.has_source

    badges = []
    eff = fn.avg_branch_efficiency
    if eff < 0.9:
        badges.append(f'<span class="badge danger">Avg eff {_fmt_pct(eff)}</span>')
    if fn.total_bank_conflicts > 0:
        badges.append(f'<span class="badge warn">{fn.total_bank_conflicts:,} bank conflicts</span>')

    if has_src:
        tabs = [("stats", "Stats"), ("split", "Source \u2194 Assembly"), ("hot", "Hotspots")]
    else:
        tabs = [("stats", "Stats"), ("asm", "Assembly"), ("hot", "Hotspots")]

    tab_btns = "".join(
        f'<button class="tab-btn{"  active" if i == 0 else ""}" '
        f'data-tab="{tab}" onclick="switchTab(\'{kid}\',\'{tab}\')">{label}</button>'
        for i, (tab, label) in enumerate(tabs)
    )

    itype = _render_itype_chart(fn)
    stats_content = (
        _render_stat_pills(fn)
        + (('<h3>Instruction type distribution (by exec count)</h3>' + itype) if itype else '')
    )

    tab_panes = []
    for i, (tab, _label) in enumerate(tabs):
        active = " active" if i == 0 else ""
        if tab == "stats":
            content = stats_content
        elif tab == "split":
            content = _render_split_view(fn, source_files)
        elif tab == "asm":
            content = _render_asm_table(fn)
        else:
            content = _render_hotspots(fn)
        tab_panes.append(f'<div id="{kid}-tab-{tab}" class="tab-pane{active}">{content}</div>')

    return f"""
<section class="card kernel-section" id="{kid}">
  <div class="kernel-header">
    <button class="back-btn" onclick="showSummary()">\u2190 Kernels</button>
    <span class="kernel-title">Launch {fn.launch_id}: {_esc(fn.demangled_name)}</span>
    {"".join(badges)}
  </div>
  <p style="font-family:monospace;font-size:11px;color:#aaa;margin-bottom:14px">{_esc(fn.name)}</p>
  <div class="tabs">{tab_btns}</div>
  {"".join(tab_panes)}
</section>
"""






def _render_summary(report: Report) -> str:
    rows = []
    for fn in report.functions:
        kid = _kernel_id(fn)
        eff = fn.avg_branch_efficiency
        eff_color = _efficiency_rgb(eff)
        bc = fn.total_bank_conflicts
        src_indicator = "\u2713" if fn.has_source else "\u2014"
        rows.append(
            f'<tr onclick="showKernel(\'{kid}\')">'
            f'<td style="text-align:center;font-weight:600">{fn.launch_id}</td>'
            f'<td class="name-cell" title="{_esc(fn.name)}">'
            f'<b>{_esc(fn.demangled_name)}</b><br>'
            f'<span style="color:#aaa;font-size:11px">{_esc(fn.name)}</span></td>'
            f'<td>{_fmt_exec(fn.total_exec_count)}</td>'
            f'<td><span style="color:{eff_color};font-weight:600">{_fmt_pct(eff)}</span></td>'
            f'<td>{"&mdash;" if bc == 0 else f"<b style=color:#d32f2f>{bc:,}</b>"}</td>'
            f'<td>{src_indicator}</td>'
            f'<td>{len(fn.instructions)}</td>'
            f'</tr>'
        )
    return f"""
<section class="card" id="summary-section">
  <h2>Kernel Summary</h2>
  <div class="table-scroll">
  <table class="summary-table">
    <thead><tr>
      <th>Launch</th><th>Kernel</th><th>Total Executions</th><th>Avg Branch Eff</th>
      <th>Bank Conflicts</th><th>Source</th><th>Unique PCs</th>
    </tr></thead>
    <tbody>{"".join(rows)}</tbody>
  </table>
  </div>
  <p style="margin-top:10px;font-size:11px;color:#aaa">Click a row to inspect the kernel.</p>
</section>
"""






def render_html(report: Report) -> str:
    kernel_sections = "\n".join(
        _render_kernel_section(fn, report.source_files) for fn in report.functions
    )
    summary = _render_summary(report)
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    n_kernels = len(report.functions)
    n_records = sum(sum(i.issue_count for i in fn.instructions) for fn in report.functions)

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{_esc(report.title)}</title>
{_AGGRID_CDN}
<style>{_CSS}</style>
</head>
<body>
<header>
  <h1>{_esc(report.title)}</h1>
  <span class="meta">{n_kernels} kernel(s) &middot; {_fmt_exec(n_records)} executions &middot; generated {_esc(ts)}</span>
</header>
<div class="container">
{summary}
{kernel_sections}
</div>
<script>{_JS}</script>
</body>
</html>
"""
