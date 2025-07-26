#!/bin/bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --partition=draco
#SBATCH --time=10:00:00
#SBATCH --job-name="fractal_client"
#SBATCH --output=%x_%j.out
#SBATCH --error=%x_%j.err

set -euo pipefail

FRACTALDIR="$HOME/fractal_pcad"
cd $FRACTALDIR

make textual LOG_LEVEL=LOG_FULL

CSV_FILE="projeto_experimental_francisco.csv"
RESULTS_DIR="results"

HOST=cei1
PORT=9191

echo "Will connect to coordinator at $HOST:$PORT"

# Try to connect to coordinator to check if it's available
check_coordinator() {
    nc -z "$HOST" "$PORT" 2>/dev/null
}

wait_for_coordinator() {    
    local tries=0
    while ! check_coordinator; do
        echo "Waiting for coordinator..."
        sleep 1
        tries=$((tries + 1))
        if [ "$tries" -ge 100 ]; then
            echo "Timeout: coordinator did not start listening after 100 seconds."
            return 1
        fi
    done
    echo "Coordinator ready"
}

mapfile -t csv_lines < <(tail -n +2 "$CSV_FILE")

experiment_count=1
for line in "${csv_lines[@]}"; do
    [[ -z "$line" ]] && continue # skip empty

    IFS=',' read -r nodes granularity max_depth screen_width screen_height llx lly urx ury difficulty blocks <<< "$line"

    echo "============================="
    echo "Client experiment $experiment_count:"
    echo "    Nodes: $nodes"
    echo "    Granularity: $granularity"
    echo "    Difficulty: $difficulty"
    echo "    Depth: $max_depth"
    echo "    Coords: LL=($llx, $lly), UR=($urx, $ury)"
    echo "    Resolution: ${screen_width}x${screen_height}"
    echo "    Repeat: ${blocks}"

    if ! wait_for_coordinator; then
        exit 1
    fi

    echo "Starting client for experiment $experiment_count..."
    ./bin/textual "$HOST" "$PORT" "$granularity" "$max_depth" "$screen_width" "$screen_height" \
                  "$llx" "$lly" "$urx" "$ury"

    client_status=$?
    echo "Client finished experiment $experiment_count with status $client_status."

    blocks_cleaned="${blocks//./}"
    OUTPUT_DIR="$RESULTS_DIR/${difficulty}/n${nodes}/g${granularity}/r${blocks_cleaned}"

    mv "client_log.txt" "$OUTPUT_DIR"

    echo "Results saved to $OUTPUT_DIR"

    experiment_count=$((experiment_count + 1))

    sleep 1
done

echo "All experiments done."
