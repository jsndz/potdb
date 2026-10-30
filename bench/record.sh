#!/bin/sh
set -e

LABEL="${1:-snapshot}"
HISTORY_DIR="bench/history"
DOC_FILE="docs/benchmark_history.md"
TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")
DATE_KEY=$(date +"%Y%m%d_%H%M%S")
SAFE_LABEL=$(echo "$LABEL" | tr ' /:;#*?' '_______')
RAW_METRICS_FILE="${HISTORY_DIR}/.tmp_raw.json"
SNAPSHOT_FILE="${HISTORY_DIR}/${DATE_KEY}_${SAFE_LABEL}.json"

mkdir -p "$HISTORY_DIR"
mkdir -p "$(dirname "$DOC_FILE")"

./build/bench_ptdb "$RAW_METRICS_FILE"

LATEST_BEFORE=$(ls -1t "$HISTORY_DIR"/*.json 2>/dev/null | grep -v "$RAW_METRICS_FILE" | grep -v "$SNAPSHOT_FILE" | head -n 1 || true)

json_val() {
    FILE="$1"
    KEY="$2"
    if [ ! -f "$FILE" ]; then
        echo ""
        return
    fi
    awk -v k="$KEY" '
    BEGIN { FS=":" }
    $0 ~ "\"" k "\"" {
        val = $2
        gsub(/^[ \t\r\n",]+|[ \t\r\n",]+$/, "", val)
        print val
        exit
    }' "$FILE"
}

calc_delta() {
    NEW_V="$1"
    OLD_V="$2"
    if [ -z "$OLD_V" ] || [ "$OLD_V" = "0" ] || [ -z "$NEW_V" ]; then
        echo "N/A"
        return
    fi
    awk -v n="$NEW_V" -v o="$OLD_V" 'BEGIN {
        d = ((n - o) / o) * 100.0;
        if (d > 0) printf "+%.2f%%", d;
        else printf "%.2f%%", d;
    }'
}

format_delta() {
    D="$1"
    IS_LOWER_BETTER="$2"
    if [ "$D" = "N/A" ]; then
        echo "-"
        return
    fi
    VAL=$(echo "$D" | tr -d '+%')
    if [ "$IS_LOWER_BETTER" = "1" ]; then
        IS_IMPROVED=$(awk -v v="$VAL" 'BEGIN { print (v < 0) ? 1 : 0 }')
    else
        IS_IMPROVED=$(awk -v v="$VAL" 'BEGIN { print (v > 0) ? 1 : 0 }')
    fi
    if [ "$IS_IMPROVED" = "1" ]; then
        echo "**$D** 🚀"
    else
        echo "$D"
    fi
}

W100=$(json_val "$RAW_METRICS_FILE" "write_tps_100")
W500=$(json_val "$RAW_METRICS_FILE" "write_tps_500")
W1000=$(json_val "$RAW_METRICS_FILE" "write_tps_1000")
W5000=$(json_val "$RAW_METRICS_FILE" "write_tps_5000")
W10K=$(json_val "$RAW_METRICS_FILE" "write_tps_10000")
W50K=$(json_val "$RAW_METRICS_FILE" "write_tps_50000")
READ=$(json_val "$RAW_METRICS_FILE" "read_tps")
MB=$(json_val "$RAW_METRICS_FILE" "read_mb_s")
A2D=$(json_val "$RAW_METRICS_FILE" "accessor_2d_ns")
A3D=$(json_val "$RAW_METRICS_FILE" "accessor_3d_ns")

cat << EOF > "$SNAPSHOT_FILE"
{
  "label": "$LABEL",
  "timestamp": "$TIMESTAMP",
  "metrics": {
    "write_tps_100": $W100,
    "write_tps_500": $W500,
    "write_tps_1000": $W1000,
    "write_tps_5000": $W5000,
    "write_tps_10000": $W10K,
    "write_tps_50000": $W50K,
    "read_tps": $READ,
    "read_mb_s": $MB,
    "accessor_2d_ns": $A2D,
    "accessor_3d_ns": $A3D
  }
}
EOF
rm -f "$RAW_METRICS_FILE"

if [ -n "$LATEST_BEFORE" ] && [ -f "$LATEST_BEFORE" ]; then
    PREV_LABEL=$(json_val "$LATEST_BEFORE" "label")
    PREV_W100=$(json_val "$LATEST_BEFORE" "write_tps_100")
    PREV_W500=$(json_val "$LATEST_BEFORE" "write_tps_500")
    PREV_W50K=$(json_val "$LATEST_BEFORE" "write_tps_50000")
    PREV_READ=$(json_val "$LATEST_BEFORE" "read_tps")
    PREV_MB=$(json_val "$LATEST_BEFORE" "read_mb_s")
    PREV_A2D=$(json_val "$LATEST_BEFORE" "accessor_2d_ns")
    PREV_A3D=$(json_val "$LATEST_BEFORE" "accessor_3d_ns")
else
    PREV_LABEL="Baseline"
    PREV_W100=""
    PREV_W500=""
    PREV_W50K=""
    PREV_READ=""
    PREV_MB=""
    PREV_A2D=""
    PREV_A3D=""
fi

D_W100=$(calc_delta "$W100" "$PREV_W100")
D_W500=$(calc_delta "$W500" "$PREV_W500")
D_W50K=$(calc_delta "$W50K" "$PREV_W50K")
D_READ=$(calc_delta "$READ" "$PREV_READ")
D_MB=$(calc_delta "$MB" "$PREV_MB")
D_A2D=$(calc_delta "$A2D" "$PREV_A2D")
D_A3D=$(calc_delta "$A3D" "$PREV_A3D")

F_W100=$(format_delta "$D_W100" 0)
F_W500=$(format_delta "$D_W500" 0)
F_W50K=$(format_delta "$D_W50K" 0)
F_READ=$(format_delta "$D_READ" 0)
F_MB=$(format_delta "$D_MB" 0)
F_A2D=$(format_delta "$D_A2D" 1)
F_A3D=$(format_delta "$D_A3D" 1)

echo ""
echo "=================================================="
echo "          BENCHMARK COMPARISON REPORT             "
echo "=================================================="
printf "Snapshot:  %s (%s)\n" "$LABEL" "$TIMESTAMP"
printf "Baseline:  %s\n" "$PREV_LABEL"
echo "--------------------------------------------------"
printf "%-32s | %-12s | %-12s | %-12s\n" "Metric" "Previous" "Current" "Delta"
echo "---------------------------------+--------------+--------------+-------------"
printf "%-32s | %-12s | %-12s | %-12s\n" "Write TPS (max_points=100)" "${PREV_W100:-N/A}" "$W100" "$D_W100"
printf "%-32s | %-12s | %-12s | %-12s\n" "Write TPS (max_points=500)" "${PREV_W500:-N/A}" "$W500" "$D_W500"
printf "%-32s | %-12s | %-12s | %-12s\n" "Write TPS (max_points=50k)" "${PREV_W50K:-N/A}" "$W50K" "$D_W50K"
printf "%-32s | %-12s | %-12s | %-12s\n" "Sequential Scan (pts/s)" "${PREV_READ:-N/A}" "$READ" "$D_READ"
printf "%-32s | %-12s | %-12s | %-12s\n" "Read Bandwidth (MB/s)" "${PREV_MB:-N/A}" "$MB" "$D_MB"
printf "%-32s | %-12s | %-12s | %-12s\n" "2D Accessor Latency (ns)" "${PREV_A2D:-N/A}" "$A2D" "$D_A2D"
printf "%-32s | %-12s | %-12s | %-12s\n" "3D Accessor Latency (ns)" "${PREV_A3D:-N/A}" "$A3D" "$D_A3D"
echo "=================================================="

if [ ! -f "$DOC_FILE" ]; then
    cat << 'EOF' > "$DOC_FILE"
# PTDB Benchmark Evolution History

This living document tracks empirical benchmark changes across feature implementations and refactors.

| Date & Time | Feature / Milestone | Write (500 pts) | Write (50k pts) | Read (pts/s) | Read Bandwidth | 2D Accessor |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
EOF
fi

echo "| $TIMESTAMP | **$LABEL** | $W500 ($F_W500) | $W50K ($F_W50K) | $READ ($F_READ) | ${MB} MB/s ($F_MB) | ${A2D} ns ($F_A2D) |" >> "$DOC_FILE"

echo "Snapshot recorded to $SNAPSHOT_FILE"
echo "Updated $DOC_FILE"
