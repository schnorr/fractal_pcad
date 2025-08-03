import sys
import os
import pandas as pd
import re

def parse_log_line(line):
    # Take log line and transform into a (tag, value) pair
    # Lines have [tag]: float format
    match = re.match(r'\[([^\]]+)\]:\s+([0-9.]+)', line.strip())
    if match:
        return match.group(1), float(match.group(2))
    return None, None

def parse_log_lines(file_path, prefix):
    metrics = {}
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            for line in f:
                tag, value = parse_log_line(line)
                if tag and value is not None:
                    metrics[f'{prefix}_{tag.lower()}'] = value
    return metrics

def parse_worker_line(line):
    # Take worker log line and extract (log, compute time, pixel_count, iterations)
    match = re.match(r'\[(WORKER_\d+_(?:PAYLOAD|TOTAL))\]:\s+([0-9.]+),\s+([0-9]+),\s+([0-9]+)', line.strip())
    if match:
        tag = match.group(1)
        compute_time = float(match.group(2))
        pixel_count = int(match.group(3))
        iterations = int(match.group(4))
        return tag, compute_time, pixel_count, iterations
    return None, None, None, None

def parse_worker_logs(repeat_path, difficulty, num_nodes, granularity, trial_id):
    worker_payloads = []
    worker_totals = []
        
    worker_files = [f for f in os.listdir(repeat_path) if f.startswith('worker_') and f.endswith('.txt')]
    
    for worker_file in worker_files:
        worker_path = os.path.join(repeat_path, worker_file)
        
        worker_num_match = re.match(r'worker_(\d+)\.txt', worker_file)
        worker_num = int(worker_num_match.group(1))
        
        with open(worker_path, 'r') as f:
            lines = f.readlines()
            parsed_lines = 0
            for line_num, line in enumerate(lines, 1):
                if line.strip():
                    tag, compute_time, pixel_count, iterations = parse_worker_line(line)
                    if tag:
                        base_row = {
                            'difficulty': difficulty,
                            'num_nodes': num_nodes,
                            'granularity': granularity,
                            'trial_id': trial_id,
                            'worker_id': worker_num,
                            'compute_time': compute_time,
                            'pixel_count': pixel_count,
                            'iterations': iterations
                        }
                        
                        if 'PAYLOAD' in tag:
                            worker_payloads.append(base_row)
                        elif 'TOTAL' in tag:
                            worker_totals.append(base_row)

    return worker_payloads, worker_totals

def process_results(results_dir):
    results = []
    all_worker_payloads = []
    all_worker_totals = []
    
    for difficulty_dir in os.listdir(results_dir):
        difficulty_path = os.path.join(results_dir, difficulty_dir)
        if not os.path.isdir(difficulty_path):
            continue
            
        for nodes_dir in os.listdir(difficulty_path):
            nodes_path = os.path.join(difficulty_path, nodes_dir)
            if not os.path.isdir(nodes_path):
                continue
                
            for granularity_dir in os.listdir(nodes_path):
                granularity_path = os.path.join(nodes_path, granularity_dir)
                if not os.path.isdir(granularity_path):
                    continue
                    
                for repeat_dir in os.listdir(granularity_path):
                    repeat_path = os.path.join(granularity_path, repeat_dir)
                    if not os.path.isdir(repeat_path):
                        continue
                    
                    print(f"Processing: {difficulty_dir}/{nodes_dir}/{granularity_dir}/{repeat_dir}")
                    
                    difficulty = difficulty_dir
                    
                    if nodes_dir.startswith('n') and nodes_dir[1:].isdigit():
                        num_nodes = int(nodes_dir[1:])
                    else:
                        num_nodes = nodes_dir
                    
                    if granularity_dir.startswith('g') and granularity_dir[1:].isdigit():
                        granularity = int(granularity_dir[1:])
                    else:
                        granularity = granularity_dir 

                    if repeat_dir.startswith('r') and repeat_dir[1:].isdigit():
                        repeat = int(repeat_dir[1:])
                    else:
                        repeat = repeat_dir 
                                        
                    client_file = os.path.join(repeat_path, 'client_log.txt')
                    coordinator_file = os.path.join(repeat_path, 'coordinator_log.txt')
                    
                    client_metrics = parse_log_lines(client_file, "client")
                    coord_metrics = parse_log_lines(coordinator_file, "coord")
                    
                    result_row = {
                        'difficulty': difficulty,
                        'num_nodes': num_nodes,
                        'granularity': granularity,
                        'trial_id': repeat,
                        **client_metrics,
                        **coord_metrics,
                    }
                    
                    results.append(result_row)

                    worker_payloads, worker_totals = parse_worker_logs(
                        repeat_path, difficulty, num_nodes, granularity, repeat
                    )
                    
                    all_worker_payloads.extend(worker_payloads)
                    all_worker_totals.extend(worker_totals)
                    
    df = pd.DataFrame(results)
    df = df.sort_values(['difficulty', 'num_nodes', 'granularity', 'trial_id'])

    worker_payloads_df = pd.DataFrame(all_worker_payloads)
    worker_payloads_df = worker_payloads_df.sort_values(['difficulty', 'num_nodes', 'granularity', 'trial_id', 'worker_id'])
    
    worker_totals_df = pd.DataFrame(all_worker_totals)
    worker_totals_df = worker_totals_df.sort_values(['difficulty', 'num_nodes', 'granularity', 'trial_id', 'worker_id'])

    return df, worker_payloads_df, worker_totals_df

if __name__ == "__main__":
    if len(sys.argv) > 1:
        results_dir = sys.argv[1]
    else:
        print("Usage: python process_client_server_results.py <results_directory>")
        sys.exit(1)

    df, worker_payloads_df, worker_totals_df = process_results(results_dir)
    print(f"Processed {len(df)} trials")
    
    print("\nFirst few rows of main results:")
    print(df.head())

    df.to_csv('experiment_results.csv', index=False, float_format='%.9f')
    print(f"Saved experiment_results.csv ({len(df)} rows)")

    worker_payloads_df.to_csv('worker_payloads.csv', index=False, float_format='%.9f')
    print(f"Saved worker_payloads.csv ({len(worker_payloads_df)} rows)")

    worker_totals_df.to_csv('worker_totals.csv', index=False, float_format='%.9f')
    print(f"Saved worker_totals.csv ({len(worker_totals_df)} rows)")

    