options(crayon.enabled=FALSE)
library(DoE.base)
library(tidyverse)

fator_granularity = c(10, 120)
fator_nodes = 1:2
fator_coordinates = c("easy", "default", "hard")

fac.design(nfactors = 3,
           replications = 2,
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

  # palceholder values for now
  mutate(
    max_depth = case_when(
      coordinates == "easy" ~ 1024,
      coordinates == "default" ~ 1024,
      coordinates == "hard" ~ 1024
    ),
    lower_left_x = case_when(
      coordinates == "easy" ~ -2.0,
      coordinates == "default" ~ -2.0,
      coordinates == "hard" ~ -2.0
    ),
    lower_left_y = case_when(
      coordinates == "easy" ~ -1.125,
      coordinates == "default" ~ -1.125,
      coordinates == "hard" ~ -1.125
    ),
    upper_right_x = case_when(
      coordinates == "easy" ~ 2.0,
      coordinates == "default" ~  2.0,
      coordinates == "hard" ~  2.0
    ),
    upper_right_y = case_when(
      coordinates == "easy" ~ 1.125,
      coordinates == "default" ~ 1.125,
      coordinates == "hard" ~ 1.125
    )
  ) |>
  mutate_at(vars(granularity:upper_right_y), as.character) |>
  select(nodes, granularity, max_depth,
         screen_width, screen_height,
         lower_left_x, lower_left_y,
         upper_right_x, upper_right_y, difficulty, 
         Blocks) |>
  write_csv("projeto_experimental_francisco.csv", progress=FALSE)