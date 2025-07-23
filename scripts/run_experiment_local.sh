#!/bin/bash

is_port_free() {
    local port=$1
    ! nc -z localhost "$port" 2>/dev/null
}

CSV_FILE="projeto_experimental_francisco.csv"
HOST="localhost"
PORT=5000 
COORDINATOR="./bin/coordinator"
CLIENT="./bin/textual"
EXPERIMENT_DIR="experiments"
CLIENT_OUTPUT_FILE="client_output.txt"
SERVER_OUTPUT_FOLDER="server_output"

mkdir -p "$EXPERIMENT_DIR"

mapfile -t csv_lines < <(tail -n +2 "$CSV_FILE")

for line in "${csv_lines[@]}"; do
    [[ -z "$line" ]] && continue # skip empty

    IFS=',' read -r nodes granularity max_depth screen_width screen_height llx lly urx ury difficulty blocks <<< "$line"

    echo "============================="
    echo "Running experiment:"
    echo "    Nodes: $nodes"
    echo "    Granularity: $granularity"
    echo "    Difficulty: $difficulty"
    echo "    Depth: $max_depth"
    echo "    Coords: LL=($llx, $lly), UR=($urx, $ury)"
    echo "    Resolution: ${screen_width}x${screen_height}"
  
    # Check if port is available before launching coordinator
    while ! is_port_free "$PORT"; do
        echo "Port $PORT is busy, waiting..."
        leep 1
    done

    # Launch coordinator
    mpirun -n $((nodes * 2)) --quiet --output-filename "$SERVER_OUTPUT_FOLDER" "$COORDINATOR" "$PORT" &> /dev/null &
    coord_pid=$!

    # Wait for coordinator to open port before launching client
    while is_port_free "$PORT"; do
        echo "Waiting for coordinator to start before launching client..."
        sleep 1
    done

    # Start client
    "$CLIENT" "$HOST" "$PORT" "$granularity" "$max_depth" "$screen_width" "$screen_height" \
               "$llx" "$lly" "$urx" "$ury" &> "$CLIENT_OUTPUT_FILE" &
    client_pid=$!

    wait "$client_pid"
    client_status=$?

    wait "$coord_pid"
    coord_status=$?

    echo "Client exited with status $client_status."
    echo "Coordinator exited with status $coord_status."

    # Remove '.' from blocks
    blocks_cleaned="${blocks//./}"
    DEST_FOLDER="$EXPERIMENT_DIR/${nodes}/${granularity}/${difficulty}/${blocks_cleaned}"
    # Store results
    mkdir -p "$DEST_FOLDER"
    mv "$CLIENT_OUTPUT_FILE" "$DEST_FOLDER/"
    mv "$SERVER_OUTPUT_FOLDER" "$DEST_FOLDER/"

    echo "Results saved."
    echo "============================="
done

echo "All experiments done."
