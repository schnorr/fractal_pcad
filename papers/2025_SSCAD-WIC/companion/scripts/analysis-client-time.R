#!/usr/bin/Rscript
options(crayon.enabled=FALSE)
suppressMessages(library(tidyverse))

meu_estilo <- function() {
  list(
    theme_bw(base_size = 14),
    theme(
      legend.title = element_blank(),
      plot.margin = unit(c(0, 0, 0, 0), "cm"),
      legend.spacing = unit(1, "mm"),
      legend.position = "right",
      legend.justification = "left",
      legend.box.spacing = unit(0, "pt"),
      legend.box.margin = margin(0, 0, 0, 0),
      axis.text.x = element_text(angle=45, vjust=1, hjust=1)    
    ))
}

args <- commandArgs(trailingOnly = TRUE)

if (length(args) < 1) {
  stop("Missing argument: Path to the CSV with timing data.")
}

csv_path <- args[1]

df <- read_csv(csv_path, progress = FALSE, show_col_types = FALSE) |>
  rename(case = difficulty) |>
  mutate(
    case = factor(case, levels = c("easy", "default", "hard")),
    num_workers = 24 * num_nodes - 1 # a single process is the coordinator
  )

# Baseline at 1 node for each case/granularity
baselines <- df |>
  filter(num_nodes == 1) |>
  group_by(case, granularity) |>
  summarise(baseline_time = mean(client_dequeue_all), .groups = "drop")

# Get time average and std dev grouped by case, granularity, nodes and num_workers
agg_df <- df |>
  group_by(case, granularity, num_nodes, num_workers) |>
  summarise(
    time_mean = mean(client_dequeue_all),
    time_sd = sd(client_dequeue_all),
    time_se = sd(client_dequeue_all) / sqrt(n()), # For confidence intervals
    .groups = "drop"
  ) |>
  # Compute speedup relative to baseline time, efficiency relative to number of workers
  left_join(baselines, by = c("case", "granularity")) |>
  mutate(
    speedup_mean = baseline_time / time_mean,
    ideal_speedup = num_workers / 23,  # Ideal speedup relative to baseline of 23 workers
    eff_mean = speedup_mean / ideal_speedup  # Efficiency relative to baseline of 23 workers
  )

# Time plot with 95% confidence error bars
time_plot <- ggplot(agg_df, aes(x = num_nodes, y = time_mean, 
                                color = factor(granularity), group = granularity)) +
  geom_line(linewidth = 0.7) +
  geom_errorbar(aes(ymin = time_mean - 1.96 * time_se, ymax = time_mean + 1.96 * time_se), 
                width = 0.2, alpha = 0.7) +
  geom_point(size = 2) +
  facet_wrap(~ case) +
  labs(x = "Node Count", y = "Time (s)", color = "Gran.") +
  xlim(0, NA) +
  meu_estilo() +
  theme(legend.title = element_text())

# Speedup plot with ideal speedup line
speedup_plot <- ggplot(agg_df, aes(x = num_nodes, y = speedup_mean, 
                                   color = factor(granularity), group = granularity)) +
  geom_line(linewidth = 0.7) +
  geom_point(size = 2) +
  # Add ideal speedup line (baseline at 23 workers)
  geom_line(aes(x = num_nodes, y = ideal_speedup), color = "black", linetype = "dashed", 
            linewidth = 0.5, alpha = 0.7) +
  facet_wrap(~ case) +
  labs(x = "Node Count", y = "Speedup", color = "Gran.") +
  xlim(0, NA) +
  meu_estilo() +
  theme(legend.title = element_text())

# Efficiency plot
efficiency_plot <- ggplot(agg_df, aes(x = num_nodes, y = eff_mean, 
                                      color = factor(granularity), group = granularity)) +
  geom_line(linewidth = 0.7) +
  geom_point(size = 2) +
  geom_line(aes(y = 1.0), linetype = "dashed", color = "black", alpha = 0.7, linewidth = 0.5) +
  facet_wrap(~ case) +
  labs(x = "Node Count", y = "Efficiency", color = "Gran.") +
  xlim(0, NA) +
  meu_estilo() +
  theme(legend.title = element_text())

ggsave("client_time.png", plot = time_plot, dpi = 300, width = 6.5, height = 2.5)
ggsave("client_speedup.png", plot = speedup_plot, dpi = 300, width = 6.5, height = 2.5)
ggsave("client_efficiency.png", plot = efficiency_plot, dpi = 300, width = 6.5, height = 2.5)