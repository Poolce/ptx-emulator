#!/bin/bash
# run_gpu.sh — benchmark all 7 kernels on real GPU (A100 / V100 / any CUDA GPU)
#
# Usage:
#   bash run_gpu.sh [--ncu]     # --ncu enables Nsight Compute hardware counters
#
# Output:
#   results/gpu_results.csv     — raw CSV
#   results/gpu_table.tex       — LaTeX table fragment ready to paste

set -euo pipefail
export LC_ALL=C
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/src"
BIN="$SCRIPT_DIR/bin"
RES="$SCRIPT_DIR/results"
mkdir -p "$BIN" "$RES"

# ── detect GPU ────────────────────────────────────────────────────────────────
if ! command -v nvidia-smi &>/dev/null; then
    echo "ERROR: nvidia-smi not found. No GPU available." >&2; exit 1
fi
GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)
CUDA_ARCH=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader | head -1 | tr -d .)
NVCC_ARCH="sm_${CUDA_ARCH}"
echo "GPU: $GPU_NAME  arch=$NVCC_ARCH"

USE_NCU=0
[[ "${1:-}" == "--ncu" ]] && USE_NCU=1

# ── compile ───────────────────────────────────────────────────────────────────
NVCC_FLAGS="-O3 -arch=$NVCC_ARCH --cudart=shared -lineinfo"
echo "Compiling..."
for f in vadd gelu softmax mmul attention fft cg; do
    nvcc $NVCC_FLAGS "$SRC/${f}_bench.cu" -o "$BIN/${f}_bench" 2>/dev/null
    echo "  ${f}_bench OK"
done

# ── kernel metadata (FLOP/byte counts baked in source comments) ───────────────
declare -A FLOPS=( [vadd]=16384      [gelu]=163840   [softmax]=49152
                   [mmul]=4194304   [attention]=8437760
                   [fft]=204800     [cg]=696320 )
declare -A BYTES=( [vadd]=98304     [gelu]=131072   [softmax]=131072
                   [mmul]=196608    [attention]=327680
                   [fft]=131072     [cg]=1474560 )
declare -A SIZES=( [vadd]="N=8192"  [gelu]="N=16384"    [softmax]="512×32"
                   [mmul]="128×128" [attention]="S=D=128"
                   [fft]="N=32,B=256" [cg]="N=4096,it=10" )

# ── roofline for detected GPU ─────────────────────────────────────────────────
# Peak values: A100-SXM4: FP32=19.5T, BW=2.0T; V100-SXM2: FP32=15.7T, BW=0.9T
# Detect from name (rough heuristic)
PEAK_FP32=15700  # GFLOP/s default (V100)
PEAK_BW=900      # GB/s default (V100)
if echo "$GPU_NAME" | grep -qi "A100"; then PEAK_FP32=19500; PEAK_BW=2000; fi
if echo "$GPU_NAME" | grep -qi "A10[^0]"; then PEAK_FP32=31200; PEAK_BW=600; fi
if echo "$GPU_NAME" | grep -qi "A30"; then PEAK_FP32=10300; PEAK_BW=933; fi
if echo "$GPU_NAME" | grep -qi "H100"; then PEAK_FP32=51200; PEAK_BW=3350; fi
if echo "$GPU_NAME" | grep -qi "RTX 40"; then PEAK_FP32=82600; PEAK_BW=1008; fi
if echo "$GPU_NAME" | grep -qi "RTX 30"; then PEAK_FP32=35600; PEAK_BW=936; fi
echo "Roofline: FP32=${PEAK_FP32} GFLOP/s  BW=${PEAK_BW} GB/s"

# ── benchmark helper ──────────────────────────────────────────────────────────
run_kernel() {
    local f=$1
    LC_ALL=C "$BIN/${f}_bench" 2>/dev/null
}

# ── optional ncu profiling ────────────────────────────────────────────────────
ncu_metrics() {
    local f=$1
    if ! command -v ncu &>/dev/null; then echo "n/a"; return; fi
    # Run one pass with Nsight Compute; extract achieved_occupancy and dram_bw
    local out
    out=$(ncu --csv --quiet \
        --metrics "sm__throughput.avg.pct_of_peak_sustained_active,\
l1tex__t_bytes_pipe_lsu_mem_global_op_ld.sum.per_second" \
        "$BIN/${f}_bench" 2>/dev/null | grep -v "^==" | tail -3)
    echo "$out"
}

# ── run all kernels ───────────────────────────────────────────────────────────
echo ""
echo "Running benchmarks on $GPU_NAME..."
CSV="$RES/gpu_results.csv"
echo "kernel,size,time_ms,gflops_achieved,bw_gb_achieved,roofline_gflops,efficiency_pct,gpu" > "$CSV"

declare -A TIMES
for f in vadd gelu softmax mmul attention fft cg; do
    printf "  %-12s ... " "$f"
    T=$(run_kernel "$f")
    TIMES[$f]=$T
    printf "%.4f ms\n" "$T"

    F=${FLOPS[$f]}; B=${BYTES[$f]}
    GFLOPS=$(python3 -c "print(f'{$F/$T/1e6:.2f}')")
    BW_GB=$(python3 -c "print(f'{$B/$T/1e6:.2f}')")
    AI=$(python3 -c "print(f'{$F/$B:.3f}')")
    ROOF=$(python3 -c "
ai=$F/$B
peak_flops=$PEAK_FP32   # GFLOP/s
peak_bw=$PEAK_BW        # GB/s
roof = min(peak_flops, ai * peak_bw)
print(f'{roof:.1f}')
")
    EFF=$(python3 -c "print(f'{$GFLOPS/$ROOF*100:.1f}')")

    echo "$f,\"${SIZES[$f]}\",$T,$GFLOPS,$BW_GB,$ROOF,$EFF,\"$GPU_NAME\"" >> "$CSV"
done

# ── optional ncu pass ─────────────────────────────────────────────────────────
if [[ $USE_NCU -eq 1 ]]; then
    echo ""
    echo "Running Nsight Compute (one rep each)..."
    NCU_CSV="$RES/ncu_results.csv"
    echo "kernel,sm_util_pct,dram_bw_gb_s" > "$NCU_CSV"
    for f in vadd gelu softmax mmul attention fft cg; do
        if command -v ncu &>/dev/null; then
            printf "  ncu %-12s ... " "$f"
            OUT=$(ncu --csv --quiet \
                --metrics "sm__throughput.avg.pct_of_peak_sustained_active,\
dram__throughput.avg.pct_of_peak_sustained_elapsed" \
                "$BIN/${f}_bench" 2>/dev/null \
                | grep -v "^==" | grep -v "^\"Kernel" | tail -1 || true)
            SM=$(echo "$OUT" | cut -d, -f3 | tr -d '"' | xargs)
            DRAM=$(echo "$OUT" | cut -d, -f4 | tr -d '"' | xargs)
            echo "$f,${SM:-n/a},${DRAM:-n/a}" >> "$NCU_CSV"
            echo "SM=${SM:-?}%  DRAM=${DRAM:-?}%"
        fi
    done
fi

# ── produce LaTeX table fragment ──────────────────────────────────────────────
TEX="$RES/gpu_table.tex"
cat > "$TEX" << 'TEXEOF'
% GPU benchmark results — generated by run_gpu.sh
% Paste into ch8_testing.tex table tab:integration_results16k
\begin{table}[h]
  \caption{Время выполнения ядер на реальном GPU (16~КБ входных данных, медиана).
           GFLOP/s и BW вычислены из аналитических счётчиков операций/байт.
           Эффективность~--- доля от порога Roofline.}
  \label{tab:gpu_bench_16k}
  \centering
  \footnotesize
TEXEOF

echo "  \\begin{tabular}{lrrrr}" >> "$TEX"
echo "    \\toprule" >> "$TEX"
printf "    %-14s & %-8s & %-10s & %-10s & %-8s \\\\\\\\\n" \
    "\\textbf{Ядро}" "\\textbf{мс}" \
    "\\textbf{GFLOP/s}" "\\textbf{BW, GB/s}" \
    "\\textbf{Eff., \\%}" >> "$TEX"
echo "    \\midrule" >> "$TEX"

python3 - "$CSV" "$PEAK_FP32" "$PEAK_BW" << 'PYEOF' >> "$TEX"
import csv, sys, math

csv_file, peak_fp32, peak_bw = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
labels = {'vadd':r'\texttt{vadd}','gelu':r'\texttt{gelu}','softmax':r'\texttt{softmax}',
          'mmul':r'\texttt{mmul}','attention':r'\texttt{attention}',
          'fft':r'\texttt{fft}','cg':r'\texttt{conj\_grad}'}
with open(csv_file) as f:
    for row in csv.DictReader(f):
        k = row['kernel']
        t   = float(row['time_ms'])
        gf  = float(row['gflops_achieved'])
        bw  = float(row['bw_gb_achieved'])
        eff = float(row['efficiency_pct'])
        name = labels.get(k, k)
        print(f"    {name:<30} & {t:>6.3f} & {gf:>8.1f} & {bw:>8.1f} & {eff:>6.1f} \\\\")
PYEOF

cat >> "$TEX" << 'TEXEOF'
    \bottomrule
  \end{tabular}
\end{table}
TEXEOF

# ── summary ───────────────────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════════════"
printf "%-14s | %8s | %10s | %10s | %8s\n" \
    "Kernel" "Time ms" "GFLOP/s" "BW GB/s" "Eff %"
echo "─────────────────────────────────────────────────────────"
python3 - "$CSV" << 'PYEOF'
import csv, sys
with open(sys.argv[1]) as f:
    for r in csv.DictReader(f):
        print(f"{r['kernel']:<14} | {float(r['time_ms']):>8.4f} | "
              f"{float(r['gflops_achieved']):>10.2f} | "
              f"{float(r['bw_gb_achieved']):>10.2f} | "
              f"{float(r['efficiency_pct']):>8.1f}")
PYEOF
echo "════════════════════════════════════════════════════════"
echo "CSV  → $CSV"
echo "LaTeX→ $TEX"
