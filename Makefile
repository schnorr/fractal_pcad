CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

GRAFICA_OBJ := $(OBJ_DIR)/grafica.o
COORDINATOR_OBJ := $(OBJ_DIR)/coordinator.o

all: grafica coordinator

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

grafica: $(filter-out $(COORDINATOR_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/grafica $^

coordinator: $(filter-out $(GRAFICA_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/coordinator $^

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all grafica coordinator clean