# Companion Materials

This folder contains the companion materials for our paper, including scripts and data used to generate the results presented.

Commands in this README should be run from the companion directory:

```bash
cd fractal_pcad/papers/2025_SSCAD-WIC/companion/
```

## Folder Structure

```
├── scripts/
│   ├── generate_experiment_parameters.r   # Script to generate experiment parameters
│   ├── experiment_client.slurm            # Script to run experiment on client
│   ├── experiment_coordinator.slurm       # Script to run experiment on coordinator
│   ├── process_client_server_results.py   # Script to parse experiment log files
│   ├── analysis-client-time.R             # Script to generate client performance figures
│   └── analysis-workers.R                 # Script to generate worker imbalance figures
└── data/
    ├── projeto_experimental.csv           # Set of input parameters used in the experiment
    ├── experiment_results.csv             # Client/Coordinator performance results
    └── worker_totals.csv                  # Worker performance results
``` 

## Using the provided data to generate figures

The included CSVs can be used directly to reproduce figures in the paper:

- [./data/experiment_results.csv](./data/experiment_results.csv): client and coordinator timing results

- `data/worker_totals.csv`: worker performance results

### Dependencies

- **R v4.3.3** with the `tidyverse` package

### Generating figures

To generate the figures in the paper, we use scripts `scripts/analysis-client-time.R` and `scripts/analysis-workers.R`.

To generate figures 3, 4 and 5 (client performance analysis), simply run:

```bash
Rscript scripts/analysis-client-time.R data/experiment_results.csv
```

To generate figure 6 (worker load balancing), run:

```bash
Rscript scripts/analysis-workers.R data/worker_totals.csv
```

Figures will be saved to the current working directory as `.png` files.

## Rerunning the full experiment

Optionally, to rerun the full experiment, you can use the slurm scripts `scripts/experiment_client.slurm`
and `scripts/experiment_coordinator.slurm`, as well as the experiment parameters 
`data/projeto_experimental.csv`. These scripts were made to run on the PCAD cluster 
at GPPD. As such, they will need adaptation to run on a different cluster environment. 

### Dependencies

- **OpenMPI 4.1.4** in a Slurm environment to compile and run the client/server
- **Python 3.12** with `pandas` to parse resulting log files into `.csv` files

Both scripts expect the repository to be in folder `$HOME/fractal_pcad`, and the experiment
parameters to be in `$HOME/fractal_pcad/projeto_experimental.csv`. Logs are saved to folder 
`$HOME/fractal_pcad/results`, within a nested folder structure.

The resulting log files can be converted to multiple `.csv` files by using script 
`scripts/process_client_server_results.py` as follows:

```bash
python3 scripts/process_client_server_results.py <results_folder>
```

Files `experiment_results.csv`, `worker_totals.csv` and `worker_payloads.csv` will be 
saved in the working directory. `worker_payloads.csv` can be very 
large and was not used to generate any figures, and so was not included in this companion.
