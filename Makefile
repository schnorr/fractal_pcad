# your raylib location here
RAYLIB=/usr

CC = gcc
CFLAGS = -I$(RAYLIB)/include -Wall -Wextra -Iinclude -pthread -g -fsanitize=address

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

GRAFICA_OBJ := $(OBJ_DIR)/grafica.o
TEXTUAL_OBJ := $(OBJ_DIR)/textual.o
COORDINATOR_OBJ := $(OBJ_DIR)/coordinator.o

all: grafica coordinator textual

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

textual: $(filter-out $(COORDINATOR_OBJ) $(GRAFICA_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/textual $^ -lm

grafica: $(filter-out $(COORDINATOR_OBJ) $(TEXTUAL_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/grafica $^ -L$(RAYLIB)/lib/ -Wl,-rpath,$(RAYLIB)/lib -lraylib -lm

coordinator: $(filter-out $(GRAFICA_OBJ) $(TEXTUAL_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/coordinator $^ -lm

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all grafica coordinator clean
