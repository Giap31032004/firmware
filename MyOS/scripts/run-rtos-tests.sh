#!/usr/bin/env bash
set -euo pipefail

backend="renode"
max_seconds=30
renode_exe=""
qemu_exe=""
tests=()

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"

usage() {
    cat <<USAGE
Usage: bash scripts/run-rtos-tests.sh [options] [test ...]

Tests:
  delay-timeout sem-timeout suspend-delay kill-wait round-robin mutex-pi
  heap-fragmentation stack-overflow queue-timeout isr-semaphore software-timer
  api-latency context-switch timer-jitter cpu-load

Options:
  -b, --backend NAME      renode or qemu. Default: renode
  -t, --max-seconds N    Per-test emulator timeout. Default: 30
  -r, --renode PATH      Renode executable path
  -q, --qemu PATH        qemu-system-arm executable path
  -h, --help             Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--backend)
            backend="$2"
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
        -q|--qemu)
            qemu_exe="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 64
            ;;
        *)
            tests+=("$1")
            shift
            ;;
    esac
done

if [[ ${#tests[@]} -eq 0 ]]; then
    tests=(
        delay-timeout
        sem-timeout
        suspend-delay
        kill-wait
        round-robin
        mutex-pi
        heap-fragmentation
        stack-overflow
        queue-timeout
        isr-semaphore
        software-timer
        api-latency
        context-switch
        timer-jitter
        cpu-load
    )
fi

scenario_for_test() {
    case "$1" in
        delay-timeout) printf '1\n' ;;
        sem-timeout) printf '2\n' ;;
        suspend-delay) printf '3\n' ;;
        kill-wait) printf '4\n' ;;
        round-robin) printf '5\n' ;;
        mutex-pi) printf '6\n' ;;
        heap-fragmentation) printf '7\n' ;;
        stack-overflow) printf '8\n' ;;
        queue-timeout) printf '10\n' ;;
        isr-semaphore) printf '12\n' ;;
        software-timer) printf '17\n' ;;
        api-latency) printf '18\n' ;;
        context-switch) printf '19\n' ;;
        timer-jitter) printf '20\n' ;;
        cpu-load) printf '21\n' ;;
        *)
            echo "Unknown RTOS test '$1'" >&2
            exit 64
            ;;
    esac
}

match_for_test() {
    case "$1" in
        delay-timeout) printf 'delay_timeout PASS\n' ;;
        sem-timeout) printf 'semaphore_timeout PASS\n' ;;
        suspend-delay) printf 'suspend_delay PASS\n' ;;
        kill-wait) printf 'kill_wait PASS\n' ;;
        round-robin) printf 'round_robin PASS\n' ;;
        mutex-pi) printf 'mutex_priority_inheritance PASS\n' ;;
        heap-fragmentation) printf 'heap_fragmentation PASS\n' ;;
        stack-overflow) printf 'stack overflow\n' ;;
        queue-timeout) printf 'queue_timeout PASS\n' ;;
        isr-semaphore) printf 'isr_semaphore PASS\n' ;;
        software-timer) printf 'software_timer PASS\n' ;;
        api-latency) printf 'api_latency PASS\n' ;;
        context-switch) printf 'context_switch_latency PASS\n' ;;
        timer-jitter) printf 'timer_jitter PASS\n' ;;
        cpu-load) printf 'cpu_load PASS\n' ;;
        *)
            echo "Unknown RTOS test '$1'" >&2
            exit 64
            ;;
    esac
}

resolve_qemu() {
    if [[ -n "$qemu_exe" ]]; then
        printf '%s\n' "$qemu_exe"
        return
    fi

    command -v qemu-system-arm
}

run_qemu_capture() {
    local test_name="$1"
    local match_pattern="$2"
    local qemu_path
    qemu_path="$(resolve_qemu)"

    mkdir -p "$repo_root/logs"
    local timestamp log_path latest_path
    timestamp="$(date +%Y%m%d-%H%M%S)"
    log_path="$repo_root/logs/qemu-$test_name-$timestamp.log"
    latest_path="$repo_root/logs/qemu-latest.log"

    set +e
    timeout "$max_seconds" "$qemu_path" \
        -M netduinoplus2 \
        -kernel "$repo_root/build/myos.elf" \
        -serial mon:stdio \
        -nographic 2>&1 | tee "$log_path"
    local status="${PIPESTATUS[0]}"
    set -e

    cp "$log_path" "$latest_path"

    if ! grep -q "$match_pattern" "$log_path"; then
        echo "QEMU did not emit '$match_pattern' for $test_name" >&2
        exit 2
    fi

    if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
        echo "QEMU exited with status $status for $test_name" >&2
        exit "$status"
    fi
}

case "$backend" in
    renode|qemu) ;;
    *)
        echo "Invalid backend '$backend'. Use renode or qemu." >&2
        exit 64
        ;;
esac

for test_name in "${tests[@]}"; do
    scenario="$(scenario_for_test "$test_name")"
    match_pattern="$(match_for_test "$test_name")"
    echo "=== RTOS test: $test_name (scenario $scenario, backend $backend) ==="

    make -C "$repo_root" clean
    make -C "$repo_root" all "MYOS_TEST_SCENARIO=$scenario"

    if [[ "$backend" == "renode" ]]; then
        args=(
            --capture-count 1
            --pattern "$match_pattern"
            --max-seconds "$max_seconds"
        )

        if [[ -n "$renode_exe" ]]; then
            args+=(--renode "$renode_exe")
        fi

        bash "$script_dir/run-renode-capture.sh" "${args[@]}"
    else
        run_qemu_capture "$test_name" "$match_pattern"
    fi
done
