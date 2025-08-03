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

read_csv("worker_totals.csv", progress=FALSE, show_col_types=FALSE) |>
  group_by(difficulty, num_nodes, granularity, trial_id, worker_id) -> df

df <- df |>
  rename(
    case = difficulty,
    nodes = num_nodes,
    repetition = trial_id
  )

df <- df |>
  summarize(duration = sum(compute_time),
            pixels = sum(pixel_count),
            depth = sum(iterations),
            .groups = "keep")

plot <- df |>
  group_by(case, nodes, granularity, repetition) |>
  summarize(imbalance.time = max(duration) / mean(duration),
            percent.imbalance = (max(duration)/mean(duration) - 1) * 100,
            imbalance.percentage = ((max(duration) - mean(duration)) / max(duration)) * n() / (n() - 1),
            std.deviation = sqrt(sum((duration - mean(duration))**2) / (n() - 1)),
            .groups="keep") |>
  print() |>
  ggplot(aes(x = repetition, y = imbalance.percentage, color = as.factor(nodes))) +
  geom_point() +
  facet_grid(case~granularity) +
  meu_estilo() +
  theme(legend.position = "top")

ggsave("imbalance_percentage.png", plot = plot, width = 8, height = 6, dpi = 300)