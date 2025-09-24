#!/usr/bin/Rscript
options(crayon.enabled=FALSE)
suppressMessages(library(tidyverse))
suppressMessages(library(colorspace))

meu_estilo <- function() {
  list(
    theme_bw(base_size = 12),
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

read_csv(csv_path, progress=FALSE, show_col_types=FALSE) |>
  group_by(difficulty, num_nodes, granularity, trial_id, worker_id) -> df

df <- df |>
  rename(
    case = difficulty,
  )

df <- df |>
  summarize(duration = sum(compute_time),
            pixels = sum(pixel_count),
            depth = sum(iterations),
            .groups = "keep")

df <- df |>
  group_by(case, num_nodes, granularity, trial_id) |>
  summarize(imbalance.time = max(duration) / mean(duration),
            percent.imbalance = (max(duration)/mean(duration) - 1) * 100,
            imbalance.percentage = ((max(duration) - mean(duration)) / max(duration)) * n() / (n() - 1),
            std.deviation = sqrt(sum((duration - mean(duration))**2) / (n() - 1)),
            .groups="keep")

plot_data <- df |>
  group_by(case, num_nodes, granularity) |>
  summarize(
    mean_imbalance.percentage = mean(imbalance.percentage, na.rm = TRUE),
    sd_imbalance.percentage = sd(imbalance.percentage, na.rm = TRUE),
    se_imbalance.percentage = sd_imbalance.percentage / sqrt(n()),
    ci_lower = mean_imbalance.percentage - qt(0.995, df = n() - 1) * se_imbalance.percentage,
    ci_upper = mean_imbalance.percentage + qt(0.995, df = n() - 1) * se_imbalance.percentage,
    .groups="keep"
  ) |>
  mutate(case = factor(case, levels = c("easy", "default", "hard")))

plot <- plot_data |>
  ggplot(aes(x = factor(num_nodes), 
             y = mean_imbalance.percentage, 
             fill = factor(granularity))) +
  geom_col(position = position_dodge(width = 0.8)) +
  geom_errorbar(aes(ymin = ci_lower, ymax = ci_upper, color = after_scale(darken(fill, 0.4))),
                position = position_dodge(width = 0.8),
                width = 0.8,
                linewidth = 0.3,
                show.legend = FALSE) +
  facet_wrap(~case, nrow = 1) +
  meu_estilo() +
  labs(x = "Nodes", 
       y = "Imbalance Percentage",
       fill = "Gran.") +
  theme(legend.title = element_text())

ggsave("imbalance_percentage.png", plot = plot, width = 6.5, height = 2.5, dpi = 300)