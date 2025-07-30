#!/usr/bin/Rscript
options(crayon.enabled=FALSE)
suppressMessages(library(tidyverse))
suppressMessages(library(lubridate))
suppressMessages(library(readxl))

meu_estilo <- function() {
  list(
    theme_bw(base_size = 16),
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
  rename(case = difficulty)

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

# Average across trials
agg_df <- df |>
  group_by(case, granularity, num_nodes) |>
  summarise(
    time_mean = mean(client_dequeue_all),
    speedup_mean = mean(speedup),
    eff_mean = mean(efficiency),
    .groups = "drop"
  )

plot_df <- agg_df |>
  pivot_longer(
    cols = c(time_mean, speedup_mean, eff_mean),
    names_to = "metric",
    values_to = "mean"
  ) |>
  mutate(
    metric_label = recode(metric,
      "time_mean" = "Time (seconds)",
      "speedup_mean" = "Speedup",
      "eff_mean" = "Efficiency"
    ),
    metric_label = factor(metric_label, levels = c("Time (seconds)", "Speedup", "Efficiency"))
  )

p <- ggplot(plot_df, aes(x = num_nodes, y = mean,
                         color = factor(granularity), group = granularity)) +
  geom_line(linewidth = 0.8) +
  geom_point(size = 2) +
  facet_grid(metric_label ~ case, scales = "free_y") +
  labs(x = "Node Count", y = NULL, color = "Granularity") +
  scale_y_continuous(expand = expansion(mult = c(0.02, 0.05)), limits = c(0, NA)) +
  meu_estilo()

ggsave("client_metrics.png", plot = p, dpi = 300, width = 8, height = 8)