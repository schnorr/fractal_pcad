import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import sys
import os

def parallel_efficiency_graph(df, case):
    """
    Generate parallel efficiency graph (speedup / num_nodes), grouped by granularity.
    """
    fig, ax = plt.subplots(figsize=(4, 3))
    
    new_df = df.copy()
    
    # Compute baseline client time at 1 node, per granularity
    baselines = (
        new_df[new_df['num_nodes'] == 1]
        .groupby('granularity')['client_dequeue_all']
        .mean()
        .to_dict()
    )
    
    # Compute speedup = baseline / current
    new_df['speedup'] = new_df.apply(
        lambda row: baselines[row['granularity']] / row['client_dequeue_all'],
        axis=1
    )
    
    new_df['efficiency'] = new_df['speedup'] / new_df['num_nodes']
    
    sns.lineplot(data=new_df, x='num_nodes', y='efficiency', hue='granularity', 
                 palette='tab10', ax=ax, marker='o')
    
    ax.set_title(f'Node Scaling Efficiency vs Node Count - Case: {case}')
    ax.set_xlabel('Node Count')
    ax.set_ylabel('Node Efficiency (Speedup / Nodes)')
    ax.legend(title='Granularity')
    ax.set_ylim(0, 1.1)
    
    plt.tight_layout()
    plt.savefig(f'graphs/parallel_efficiency_{case}.png', dpi=300, bbox_inches='tight')
    plt.close()

def client_speedup_graph(df, case):
    """
    Graph the speedup of the client relative to 1 node, and grouped by granularity.
    """
    fig, ax = plt.subplots(figsize=(4, 3))

    new_df = df.copy()

    # Compute baseline client time at 1 node, per granularity
    baselines = (
        new_df[new_df['num_nodes'] == 1]
        .groupby('granularity')['client_dequeue_all']
        .mean()
        .to_dict()
    )

    # Compute speedup = baseline / current
    new_df['speedup'] = new_df.apply(
        lambda row: baselines[row['granularity']] / row['client_dequeue_all'],
        axis=1
    )

    sns.lineplot(data=new_df, x='num_nodes', y='speedup', hue='granularity', palette='tab10', ax=ax, marker='o')

    ax.axhline(y=1.0, linestyle='--', color='gray', linewidth=1)
    ax.set_title(f'Client speedup vs node count - Case: {case}')
    ax.set_xlabel('Node count')
    ax.set_ylabel('Speedup (relative to 1 node)')
    ax.legend(title='Granularity')

    plt.tight_layout()
    plt.savefig(f'graphs/client_speedup_{case}.png', dpi=300, bbox_inches='tight')
    plt.close()

def client_time_graph(df, case):
    """
    Average time for the client to get the full image with different node counts, grouped by granularity.
    """
    fig, ax = plt.subplots(figsize=(4, 3))

    sns.lineplot(data=df, x='num_nodes', y='client_dequeue_all', hue='granularity', palette='tab10', ax=ax, marker='o')

    ax.set_title(f'Time for client to get full fractal - Case: {case}')
    ax.set_xlabel('Node count')
    ax.set_ylabel('Time (seconds)')
    ax.legend(title='Granularity')
    
    plt.tight_layout()
    plt.savefig(f'graphs/client_dequeue_all_{case}.png', dpi=300, bbox_inches='tight')
    plt.close()


if __name__ == "__main__":
    if len(sys.argv) > 2:
        main_csv_path = sys.argv[1]
        worker_totals_csv_path = sys.argv[2]
    else:
        print("Usage: python generate_graphs.py <client/coordinator csv> <worker totals csv>")
        sys.exit(1)

    os.makedirs("graphs", exist_ok=True)
    sns.set(style="whitegrid")

    plt.rcParams.update({
        'font.size': 10,
        'axes.titlesize': 10,
        'axes.labelsize': 9,
        'xtick.labelsize': 8,
        'ytick.labelsize': 8,
        'legend.title_fontsize': 8,
        'legend.fontsize': 7,
        'figure.titlesize': 10,
        'lines.linewidth': 1.5,
        'lines.markersize': 4
    })    

    main_df = pd.read_csv(main_csv_path)
    worker_totals_df = pd.read_csv(worker_totals_csv_path)

    main_df.rename(columns={'difficulty': 'case'}, inplace=True)
    worker_totals_df.rename(columns={'difficulty': 'case'}, inplace=True)

    cases = main_df['case'].unique()

    for case in cases:
        print(f"Generating graphs for case: {case}")
        filtered_main_df = main_df[main_df['case'] == case]
        filtered_worker_totals_df = worker_totals_df[worker_totals_df['case'] == case]

        client_time_graph(filtered_main_df, case)
        client_speedup_graph(filtered_main_df, case)
        parallel_efficiency_graph(filtered_main_df, case)