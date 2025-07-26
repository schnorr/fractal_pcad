#!/bin/bash
#SBATCH --nodes=6
#SBATCH --ntasks-per-node=24
#SBATCH --partition=cei
#SBATCH --time=10:00:00
#SBATCH --job-name="fractal_server"
#SBATCH --output=%x_%j.out
#SBATCH --error=%x_%j.err

set -euo pipefail

hostname

FRACTALDIR="$HOME/fractal_pcad"

cd $FRACTALDIR

make coordinator LOG_LEVEL=LOG_FULL

CSV_FILE="projeto_experimental_francisco.csv"
HOST=cei1
PORT=9191
RESULTS_DIR="results"

HOSTFILE="hostfile"
srun -l hostname | awk '{print $2}' | sort | uniq -c | awk '{print $2 " slots=" $1}' > "$HOSTFILE"
echo "Hostfile:"
cat $HOSTFILE

echo "Coordinator will be running on $HOST:$PORT"
mkdir -p "$RESULTS_DIR"

# Get first n lines of hostfile
get_hostfile_nodes() {
    local num_nodes=$1
    head -n "$num_nodes" "$HOSTFILE"
}

np_from_hostfile() {
    local hostfile=$1
    awk -F'slots=' '{sum += $2} END {print sum}' "$hostfile"
}

# Skip csv header
mapfile -t csv_lines < <(tail -n +2 "$CSV_FILE")

experiment_count=1
for line in "${csv_lines[@]}"; do
    [[ -z "$line" ]] && continue # skip empty lines

    IFS=',' read -r nodes granularity max_depth screen_width screen_height llx lly urx ury difficulty blocks <<< "$line"
    
    echo "============================="
    echo "Experiment $experiment_count:"
    echo "    Nodes: $nodes"
    echo "    Granularity: $granularity"
    echo "    Difficulty: $difficulty"
    echo "    Depth: $max_depth"
    echo "    Coords: LL=($llx, $lly), UR=($urx, $ury)"
    echo "    Resolution: ${screen_width}x${screen_height}"
    echo "    Repeat: ${blocks}"

    # Create output directory for parameters
    blocks_cleaned="${blocks//./}"
    OUTPUT_DIR="$RESULTS_DIR/${difficulty}/n${nodes}/g${granularity}/r${blocks_cleaned}"
    mkdir -p "$OUTPUT_DIR"

    # Create temporary hostfile for given number of nodes
    EXPERIMENT_HOSTFILE="hostfile_experiment_${experiment_count}"
    get_hostfile_nodes "$nodes" > "$EXPERIMENT_HOSTFILE"
    
    echo "$EXPERIMENT_HOSTFILE"
    cat "$EXPERIMENT_HOSTFILE"
    
    NP=$(np_from_hostfile "$EXPERIMENT_HOSTFILE")
    echo "Launching coordinator on $nodes nodes with $NP CPUs"

    mpirun \
        --mca oob_tcp_if_include 192.168.30.0/24 \
        --mca btl_tcp_if_include 192.168.30.0/24 \
        --np "$NP" \
        --hostfile "$EXPERIMENT_HOSTFILE" \
        ${FRACTALDIR}/bin/coordinator $PORT 

    coordinator_status=$?
    echo "Coordinator finished experiment $experiment_count with status $coordinator_status"

    mv coordinator_log.txt worker_logs/* "$OUTPUT_DIR"

    # Prevent any leftovers
    rm -r worker_logs

    # Remove temporary hostfile
    rm -f "$EXPERIMENT_HOSTFILE"

    experiment_count=$((experiment_count + 1))

    sleep 1
done

echo "All experiments done."