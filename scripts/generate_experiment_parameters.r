options(crayon.enabled=FALSE)
library(DoE.base)
library(tidyverse)

fator_granularity = c(5, 10, 20, 40, 60, 120)
fator_nodes = 1:6
fator_coordinates = c("easy", "default", "hard")

fac.design(nfactors = 3,
           replications = 10,
           repeat.only = FALSE,
           randomize = TRUE,
           seed=0,
           nlevels=c(length(fator_granularity),
                     length(fator_nodes),
                     length(fator_coordinates)),
           factor.names=list(
             granularity = fator_granularity,
             nodes = fator_nodes,
             coordinates = fator_coordinates
           )) |>
  as_tibble() |>
  mutate(
    screen_width = 1920,
    screen_height = 1080
  ) |>

  # copy difficulty level so it's easy to identify
  mutate(difficulty = coordinates) |>

  mutate(
    max_depth = case_when(
      coordinates == "easy" ~ 1024,
      coordinates == "default" ~ 150000,
      coordinates == "hard" ~ 300000
    ),
    lower_left_x = case_when(
      coordinates == "easy" ~ "-0.005873612866112004283",
      coordinates == "default" ~ "-2.0",
      coordinates == "hard" ~ "0.250455424878192725363"
    ),
    lower_left_y = case_when(
      coordinates == "easy" ~ "-1.007323566937334392824",
      coordinates == "default" ~ "-1.125",
      coordinates == "hard" ~ "0.000015964345669804373"
    ),
    upper_right_x = case_when(
      coordinates == "easy" ~ "0.003830416542916253462",
      coordinates == "default" ~  "2.0",
      coordinates == "hard" ~  "0.250455424878194101568"
    ),
    upper_right_y = case_when(
      coordinates == "easy" ~ "-1.001865048436934870673",
      coordinates == "default" ~ "1.125",
      coordinates == "hard" ~  "0.000015964345670578497"
    )
  ) |>
  
  mutate(across(where(is.numeric), ~ format(.x, scientific = FALSE, trim = TRUE))) |>
  select(nodes, granularity, max_depth,
         screen_width, screen_height,
         lower_left_x, lower_left_y,
         upper_right_x, upper_right_y, difficulty, 
         Blocks) |>
  write_csv("projeto_experimental_francisco.csv", progress=FALSE)