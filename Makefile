SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/jsdist
OBJ := main.o jsdist.o thpool.o

CPPFLAGS := -MMD -MP -ggdb
CFLAGS	 := -std=c11 -Wall -Werror #-fsanitize=address
LDFLAGS	 := -Llib #-fsanitize=address
LDLIBS	 := -lm -pthread


.PHONY: all debug clean

all: $(EXE)

debug: export CACHE_DEBUG = 1
debug: export LSAN_OPTIONS=verbosity=1:log_threads=1
debug: $(EXE)
	@echo LSAN_OPTIONS = $$LSAN_OPTIONS
	@echo CACHE_DEBUG = $$CACHE_DEBUG


$(EXE): $(OBJ_DIR)/main.o $(OBJ_DIR)/jsdist.o $(OBJ_DIR)/thpool.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/jsdist/jsdist.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/jsdist.o: $(SRC_DIR)/jsdist/jsdist.c $(SRC_DIR)/thpool/thpool.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/thpool.o: $(SRC_DIR)/thpool/thpool.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

-include $(INCL)

jsdist.o: