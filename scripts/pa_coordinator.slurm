#!/bin/bash
#SBATCH --nodes=3
#SBATCH --partition=cei
#SBATCH --time=02:00:00
#SBATCH --job-name="fractal"
#SBATCH --output=%x_%j.out
#SBATCH --error=%x_%j.err

# fail on error
# https://gist.github.com/mohanpedala/1e2ff5661761d3abd0385e8223e16425
set -euxo pipefail

# Basic configuration
BASEEXPDIR=$HOME/pa_workspace/
FRACTALDIR=${BASEEXPDIR}/fractal_pcad/
KEY=${SLURM_JOB_NAME}_${SLURM_JOB_ID}

# GUIX (https://guix.gnu.org/cookbook/en/html_node/Setting-Up-Compute-Nodes.html)
{
    GUIX_DAEMON_SOCKET="guix://192.168.30.23"
    export GUIX_DAEMON_SOCKET
    GUIX_LOCPATH=/var/guix/profiles/per-user/root/guix-profile/lib/locale
    export GUIX_LOCPATH
    for GUIX_PROFILE in "$HOME/.config/guix/current" "$HOME/.guix-profile"
    do
	if [ -f "$GUIX_PROFILE/etc/profile" ]; then
	    . "$GUIX_PROFILE/etc/profile"
	fi
    done
    GUIX_MANIFEST=$(pwd)/${KEY}.scm
    cat > ${GUIX_MANIFEST} <<EOF
(specifications->manifest
 '("gcc-toolchain"
   "openmpi"
   "coreutils"
   "grep"
   "findutils"
   "guix"
   "make"
   "which"
   "sed"))
EOF
    #guix package -p $HOME/.guix-profile/ -m ${GUIX_MANIFEST}
}

# Fractal app compile
{
    pushd ${FRACTALDIR}
    COMPILE_SCRIPT=./compile_within_guix_env.sh 
    cat > ${COMPILE_SCRIPT} << EOF
    ldd --version
    mpicc --version
    make clean
    # disable grafica and textual compilation
    sed -i 's/all: grafica coordinator textual/all: coordinator/' Makefile
    sed -i 's/-g -fsanitize=address/-O3/' Makefile
    # compile
    make
EOF
    chmod 755 ${COMPILE_SCRIPT}
    guix shell --pure --container -m ${GUIX_MANIFEST} -- ${COMPILE_SCRIPT}
    popd
}

hostname

# Create the hostfile
HOSTFILE="${KEY}.nodes"
srun -l hostname | sort -n | awk '{print $2}' | sed 's/$/ slots=1/' > $HOSTFILE

#   --bind-to nonenone \
#   -mca btl_base_verbose 30 \
#   -mca pml_base_verbose 30 \
#   -mca oob_base_verbose 30 \

# Run
#env | awk -F= '{print "-x " $1}' | xargs \
RUN_SCRIPT=./run_within_guix_env.sh
cat > ${RUN_SCRIPT} << EOF
ldd --version
mpirun --version
mpirun \
    --mca oob_tcp_if_include 192.168.30.0/24 \
    --mca btl_tcp_if_include 192.168.30.0/24 \
    --np ${SLURM_JOB_NUM_NODES} \
    --hostfile $HOSTFILE \
    ${FRACTALDIR}/bin/coordinator \
    9191
EOF
chmod 755 ${RUN_SCRIPT}
guix shell --pure --container -m ${GUIX_MANIFEST} -- ${RUN_SCRIPT}
echo "terminou"
