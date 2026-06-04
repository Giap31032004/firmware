#!/usr/bin/env bash
set -euo pipefail

capture_count=3
match_pattern="+-----------------------------------------------+"
max_seconds=30
renode_exe=""
renode_script=""
log_dir=""

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"

usage() {
    cat <<USAGE
Usage: bash scripts/run-renode-capture.sh [options]

Options:
  -c, --capture-count N   Stop after N matching lines. Default: 3
  -p, --pattern TEXT      Line pattern to count. Default: panel footer
  -t, --max-seconds N     Stop after N seconds. Default: 30
  -r, --renode PATH       Renode executable path. Default: bundled Renode or PATH
  -s, --script PATH       Renode .resc script. Default: RenodeOfMe/stm32.resc
  -o, --log-dir PATH      Output log directory. Default: logs
  -h, --help              Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--capture-count)
            capture_count="$2"
            shift 2
            ;;
        -p|--pattern)
            match_pattern="$2"
            shift 2
            ;;
        -t|--max-seconds)
            max_seconds="$2"
            shift 2
            ;;
        -r|--renode)
            renode_exe="$2"
            shift 2
            ;;
        -s|--script)
            renode_script="$2"
            shift 2
            ;;
        -o|--log-dir)
            log_dir="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 64
            ;;
    esac
done

if [[ -z "$renode_script" ]]; then
    renode_script="$repo_root/RenodeOfMe/stm32.resc"
fi

if [[ -z "$log_dir" ]]; then
    log_dir="$repo_root/logs"
fi

resolve_renode() {
    if [[ -n "$renode_exe" ]]; then
        printf '%s\n' "$renode_exe"
        return
    fi

    local bundled="$repo_root/scripts/tools/renode/renode_1.16.1/renode"
    if [[ -x "$bundled" || -f "$bundled" ]]; then
        printf '%s\n' "$bundled"
        return
    fi

    if command -v renode >/dev/null 2>&1; then
        command -v renode
        return
    fi

    echo "Renode not found. Pass --renode PATH or add renode to PATH." >&2
    exit 127
}

renode_path="$(resolve_renode)"
mkdir -p "$log_dir"

timestamp="$(date +%Y%m%d-%H%M%S)"
log_path="$log_dir/renode-capture-$timestamp.log"
latest_path="$log_dir/renode-capture-latest.log"
monitor_source="$(dirname -- "$renode_path")/myos_monitor.log"
monitor_copy="$log_dir/renode-monitor-$timestamp.log"
monitor_latest="$log_dir/renode-monitor-latest.log"

echo "Renode: $renode_path"
echo "Script: $renode_script"
echo "Log: $log_path"
echo "CaptureCount: $capture_count, MatchPattern: $match_pattern"

matched_count=0

coproc RENODE_PROC {
    cd "$repo_root"
    "$renode_path" --console --disable-xwt "$renode_script" 2>&1
}

renode_pid="$RENODE_PROC_PID"

cleanup() {
    if kill -0 "$renode_pid" >/dev/null 2>&1; then
        kill "$renode_pid" >/dev/null 2>&1 || true
        wait "$renode_pid" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

deadline=$((SECONDS + max_seconds))

while kill -0 "$renode_pid" >/dev/null 2>&1; do
    if IFS= read -r -t 1 line <&"${RENODE_PROC[0]}"; then
        printf '%s\n' "$line" | tee -a "$log_path"

        if [[ "$line" == *"$match_pattern"* ]]; then
            matched_count=$((matched_count + 1))
            if (( capture_count > 0 && matched_count >= capture_count )); then
                break
            fi
        fi
    fi

    if (( SECONDS >= deadline )); then
        echo "MaxSeconds reached, stopping Renode." | tee -a "$log_path"
        break
    fi
done

cleanup
cp "$log_path" "$latest_path"

if [[ -f "$monitor_source" ]]; then
    cp "$monitor_source" "$monitor_copy"
    cp "$monitor_source" "$monitor_latest"
    echo "Saved monitor log copy: $monitor_copy"
    echo "Latest monitor log copy: $monitor_latest"
fi

echo "Saved log: $log_path"
echo "Latest log: $latest_path"
echo "Matched '$match_pattern': $matched_count"

if (( capture_count > 0 && matched_count < capture_count )); then
    exit 2
fi
