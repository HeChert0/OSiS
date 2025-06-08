CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -MMD -MP -g -Wno-unused-parameter -Wno-unused-variable -D_POSIX_C_SOURCE=200809L

SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release

EXEC = app

DIRS = $(BUILD_DIR) $(DEBUG_DIR) $(RELEASE_DIR)

all: debug

$(DEBUG_DIR)/main.o: $(SRC_DIR)/main.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/queue.o: $(SRC_DIR)/queue.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/$(EXEC): $(DEBUG_DIR)/main.o $(DEBUG_DIR)/queue.o | $(DEBUG_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(RELEASE_DIR)/main.o: $(SRC_DIR)/main.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 -c $< -o $@

$(RELEASE_DIR)/queue.o: $(SRC_DIR)/queue.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 -c $< -o $@

$(RELEASE_DIR)/$(EXEC): $(RELEASE_DIR)/main.o $(RELEASE_DIR)/queue.o | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 $^ -o $@

$(DIRS):
	mkdir -p $@

release: $(DIRS) $(RELEASE_DIR)/$(EXEC)
	@echo "Release сборка завершена: $(RELEASE_DIR)/$(EXEC)"

clean:
	rm -f $(DEBUG_DIR)/* $(RELEASE_DIR)/*

debug: $(DIRS) $(DEBUG_DIR)/$(EXEC)
	@echo "Debug сборка завершена: $(DEBUG_DIR)/$(EXEC)"

memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(DEBUG_DIR)/$(EXEC)

-include $(wildcard $(DEBUG_DIR)/*.d) $(wildcard $(RELEASE_DIR)/*.d)

.PHONY: all release clean debug memcheck
