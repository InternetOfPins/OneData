#!/usr/bin/env bash
# OneData standalone benchmark runner
# Usage: ./run_bench.sh [CXX=clang++] [SIZES="10 25 50 100"] [REPS=1000000]
#
# Measures:
#   compile time  — -fsyntax-only at N = 10 25 50 100
#   runtime       — get/set, Watch, StaticNumRange, forEach, find at N = 20, 50
#   binary size   — stripped runtime binary
#
# Results stored in benchmark/results/YYYY-MM-DD_HH-MM-SS.log and
# diffed against the previous run.

set -euo pipefail

BENCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONEDATA_INCLUDE="${BENCH_DIR}/../include"
HAPI_INCLUDE="${BENCH_DIR}/../../HAPI/include"
SRC="${BENCH_DIR}/bench_onedata.cpp"
RESULTS_DIR="${BENCH_DIR}/results"
mkdir -p "${RESULTS_DIR}"

TIMESTAMP=$(date '+%Y-%m-%d_%H-%M-%S')
LOG="${RESULTS_DIR}/${TIMESTAMP}.log"
TMP=$(mktemp -d)
trap 'rm -rf "${TMP}"' EXIT

: "${CXX:=g++}"
: "${SIZES:=10 25 50 100}"
: "${REPS:=1000000}"
: "${RUNTIME_SIZES:=20 50}"

FLAGS="-std=c++17 -ftemplate-depth=2000 -I${ONEDATA_INCLUDE} -I${HAPI_INCLUDE}"

# ── helpers ──────────────────────────────────────────────────────────────────

ms_now() { date +%s%3N; }

compile_ms() {
    local rc=0 t0 t1
    t0=$(ms_now)
    "${CXX}" ${FLAGS} "$@" "${SRC}" 2>/dev/null || rc=$?
    t1=$(ms_now)
    [[ ${rc} -eq 0 ]] && echo $((t1 - t0)) || echo "ERR"
}

# ── benchmark body ────────────────────────────────────────────────────────────

{
    CXX_VER=$("${CXX}" --version | head -1)
    ONEDATA_VER=$(grep '"version"' "${BENCH_DIR}/../library.json" 2>/dev/null \
                  | sed 's/.*"\([0-9][0-9.]*\)".*/\1/' || echo "?")
    HAPI_VER=$(grep '"version"' "${HAPI_INCLUDE}/../library.json" 2>/dev/null \
               | sed 's/.*"\([0-9][0-9.]*\)".*/\1/' || echo "?")

    echo "=== OneData Standalone Benchmark ==="
    echo "Date:      $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Host:      $(uname -n)"
    echo "Compiler:  ${CXX_VER}"
    echo "OneData:   ${ONEDATA_VER}"
    echo "HAPI:      ${HAPI_VER}"
    echo "Flags:     ${FLAGS}"
    echo ""

    # ── compile-time table ────────────────────────────────────────────────────

    echo "--- Compile time (ms, -fsyntax-only -O0) ---"
    echo "  flat_chain - baseline  ≈ DataDef<N> collapse cost"
    echo "  watch_stack - flat     ≈ per-level Watch modifier overhead"
    echo "  find_last  - find_first ≈ FindFirst traversal cost per step"
    echo ""

    declare -a TESTS=( baseline flat_chain watch_stack find_first find_last foreach at_tagged idx_chain at_array mapped )
    declare -a DEFS=(  TEST_BASELINE TEST_FLAT_CHAIN TEST_WATCH_STACK \
                       TEST_FIND_FIRST TEST_FIND_LAST TEST_FOREACH \
                       TEST_AT_TAGGED TEST_IDX_CHAIN TEST_AT_ARRAY TEST_MAPPED )
    read -ra SIZE_ARR <<< "${SIZES}"

    printf "%-14s" ""
    for N in "${SIZE_ARR[@]}"; do printf "%7s" "N=${N}"; done
    echo ""
    printf "%-14s" ""
    for N in "${SIZE_ARR[@]}"; do printf "%7s" "------"; done
    echo ""

    for i in "${!TESTS[@]}"; do
        printf "%-14s" "${TESTS[$i]}"
        for N in "${SIZE_ARR[@]}"; do
            ms=$(compile_ms -O0 -fsyntax-only "-DTEST_SIZE=${N}" "-D${DEFS[$i]}")
            printf "%7s" "${ms}"
        done
        echo ""
    done

    echo ""

    # ── runtime measurements ──────────────────────────────────────────────────

    echo "--- Runtime (-O2, REPS=${REPS}) ---"
    echo ""

    read -ra RT_SIZES <<< "${RUNTIME_SIZES}"
    for N in "${RT_SIZES[@]}"; do
        BIN="${TMP}/bench_od_${N}"
        echo "  N=${N}:"
        if "${CXX}" ${FLAGS} -O2 -DTEST_RUNTIME "-DTEST_SIZE=${N}" "-DREPS=${REPS}" \
                   -o "${BIN}" "${SRC}" 2>/dev/null; then
            KB=$(wc -c < "${BIN}" | awk '{printf "%.1f", $1/1024}')
            printf "    binary size: %s KB\n" "${KB}"
            "${BIN}" | sed 's/^/    /'
        else
            echo "    [compile failed]"
        fi
        echo ""
    done

} | tee "${LOG}"

# ── compare with previous run ─────────────────────────────────────────────────

PREV=$(ls -1t "${RESULTS_DIR}"/*.log 2>/dev/null | grep -vF "${LOG}" | head -1 || true)
if [[ -n "${PREV}" ]]; then
    echo ""
    echo "=== Diff vs $(basename "${PREV}") ==="
    diff "${PREV}" "${LOG}" || true
fi

echo ""
echo "Saved: ${LOG}"
