# your raylib location here
RAYLIB=/usr

ifndef LOG_LEVEL
LOG_LEVEL = LOG_BASIC
endif

CC = gcc
MPICC = mpicc
CFLAGS = -I$(RAYLIB)/include -Wall -Wextra -Iinclude -pthread -g -fsanitize=address -DLOG_LEVEL=$(LOG_LEVEL)

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

GRAFICA_OBJ := $(OBJ_DIR)/grafica.o $(OBJ_DIR)/colors.o
TEXTUAL_OBJ := $(OBJ_DIR)/textual.o
COORDINATOR_OBJ := $(OBJ_DIR)/coordinator.o

all: grafica coordinator textual

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(MPICC) $(CFLAGS) -c $< -o $@

textual: $(filter-out $(COORDINATOR_OBJ) $(GRAFICA_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/textual $^ -lm -lmpi

grafica: $(filter-out $(COORDINATOR_OBJ) $(TEXTUAL_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/grafica $^ -L$(RAYLIB)/lib/ -Wl,-rpath,$(RAYLIB)/lib -lraylib -lm -lmpi

coordinator: $(filter-out $(GRAFICA_OBJ) $(TEXTUAL_OBJ), $(OBJS))
	@mkdir -p $(BIN_DIR)
	$(MPICC) $(CFLAGS) -o $(BIN_DIR)/coordinator $^ -lm -lmpi

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all grafica coordinator clean
