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
  summarise(base_time = mean(client_dequeue_all), .groups = "drop")

df <- df |>
  left_join(baselines, by = c("case", "granularity")) |>
  mutate(
    speedup = base_time / client_dequeue_all,
    efficiency = speedup / num_nodes
  )

# Get averages and standard deviations for error bars
agg_df <- df |>
  group_by(case, granularity, num_nodes) |>
  summarise(
    time_mean    = mean(client_dequeue_all),
    time_ci      = 1.96 * sd(client_dequeue_all) / sqrt(n()),
    speedup_mean = mean(speedup),
    speedup_ci   = 1.96 * sd(speedup) / sqrt(n()),
    eff_mean     = mean(efficiency),
    eff_ci       = 1.96 * sd(efficiency) / sqrt(n()),
    .groups = "drop"
  )

create_plot <- function(data, metric, y_label) {
  mean_columns <- paste0(metric, "_mean")
  confidence_columns <- paste0(metric, "_ci")
  
  ggplot(data, aes(x = num_nodes, y = .data[[mean_columns]], 
                   color = factor(granularity), group = granularity)) +
    geom_line(linewidth = 0.7) +
    geom_errorbar(aes(ymin = .data[[mean_columns]] - .data[[confidence_columns]], 
                      ymax = .data[[mean_columns]] + .data[[confidence_columns]]),
                      width = 0.2, alpha = 0.7) +
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