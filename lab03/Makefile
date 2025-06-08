CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -MMD -MP -Wno-unused-parameter -Wno-unused-variable -D_POSIX_C_SOURCE=200809L

SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release

DEBUG_PARENT = $(DEBUG_DIR)/parent
DEBUG_CHILD  = $(DEBUG_DIR)/child
RELEASE_PARENT = $(RELEASE_DIR)/parent
RELEASE_CHILD  = $(RELEASE_DIR)/child

DIRS = $(BUILD_DIR) $(DEBUG_DIR) $(RELEASE_DIR)

all: debug

$(DEBUG_DIR)/parent.o: $(SRC_DIR)/parent.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/child.o: $(SRC_DIR)/child.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_PARENT): $(DEBUG_DIR)/parent.o | $(DEBUG_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(DEBUG_CHILD): $(DEBUG_DIR)/child.o | $(DEBUG_DIR)
	$(CC) $(CFLAGS) $< -o $@

release: $(DIRS) $(RELEASE_PARENT) $(RELEASE_CHILD)
	@echo "Release сборка завершена: $(RELEASE_PARENT) и $(RELEASE_CHILD)"

$(RELEASE_DIR)/parent.o: $(SRC_DIR)/parent.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 -c $< -o $@

$(RELEASE_DIR)/child.o: $(SRC_DIR)/child.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 -c $< -o $@

$(RELEASE_PARENT): $(RELEASE_DIR)/parent.o | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 $< -o $@

$(RELEASE_CHILD): $(RELEASE_DIR)/child.o | $(RELEASE_DIR)
	$(CC) $(CFLAGS) -O2 $< -o $@

$(DIRS):
	mkdir -p $@

clean:
	rm -f $(DEBUG_DIR)/* $(RELEASE_DIR)/*

debug: $(DIRS) $(DEBUG_PARENT) $(DEBUG_CHILD)
	@echo "Debug сборка завершена: $(DEBUG_PARENT) и $(DEBUG_CHILD)"

memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(DEBUG_PARENT)


-include $(wildcard $(DEBUG_DIR)/*.d) $(wildcard $(RELEASE_DIR)/*.d)

.PHONY: all release clean
