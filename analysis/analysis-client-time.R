#!/usr/bin/Rscript
options(crayon.enabled=FALSE)
suppressMessages(library(tidyverse))
suppressMessages(library(lubridate))
suppressMessages(library(readxl))

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

df <- read_csv("experiment_results.csv", progress=FALSE, show_col_types=FALSE) |>
  rename(case = difficulty) |>
  mutate(case = factor(case, levels = c("easy", "default", "hard"))) # explicitly order easy-default-hard

# Baseline at 1 node for each case/granularity
baselines <- df |>
  filter(num_nodes == 1) |>
  group_by(case, granularity) |>
  summarise(baseline_time = mean(client_dequeue_all), .groups = "drop")

# Get time average grouped by case, granularity and nodes
agg_df <- df |>
  group_by(case, granularity, num_nodes) |>
  summarise(
    time_mean = mean(client_dequeue_all),
    .groups = "drop"
  ) |>
  # Compute speedup/efficiency compared to baseline time
  left_join(baselines, by = c("case", "granularity")) |>
  mutate(
    speedup_mean = baseline_time / time_mean,
    eff_mean = speedup_mean / num_nodes
  )

create_plot <- function(data, metric, y_label) {
  mean_column <- paste0(metric, "_mean")
  ggplot(data, aes(x = num_nodes, y = .data[[mean_column]], 
                   color = factor(granularity), group = granularity)) +
    geom_line(linewidth = 0.7) +
    geom_point(size = 2) +
    facet_wrap(~ case) +
    labs(x = "Node Count", y = y_label, color = "Gran.") +
    meu_estilo() +
    theme(legend.title = element_text())
}

time_plot <- create_plot(agg_df, "time", "Time (s)")
speedup_plot <- create_plot(agg_df, "speedup", "Speedup")
efficiency_plot <- create_plot(agg_df, "eff", "Efficiency") +
  geom_hline(yintercept = 1.0, linetype = "dashed", color = "gray50", alpha = 0.7)

ggsave("client_time.png", plot = time_plot, dpi = 300, width = 6.5, height = 3.0)
ggsave("client_speedup.png", plot = speedup_plot, dpi = 300, width = 6.5, height = 3.0)
ggsave("client_efficiency.png", plot = efficiency_plot, dpi = 300, width = 6.5, height = 3.0)