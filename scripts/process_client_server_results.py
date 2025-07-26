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

def process_results(results_dir):
    results = []
    
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
                    
    df = pd.DataFrame(results)
    df = df.sort_values(['difficulty', 'num_nodes', 'granularity', 'trial_id'])
    
    return df

if __name__ == "__main__":
    if len(sys.argv) > 1:
        results_dir = sys.argv[1]
    else:
        print("Usage: python process_client_server_results.py <results_directory>")
        sys.exit(1)

    df = process_results(results_dir)
    print(f"Processed {len(df)} trials")
    
    print("\nFirst few rows of main results:")
    print(df.head())

    df.to_csv('experiment_results.csv', index=False)
    print(f"Saved experiment_results.csv ({len(df)} rows)")