CC = gcc

CFLAGS_DEBUG = -Wall -Wextra -std=c23 -pedantic -g -MMD -MP -D_POSIX_C_SOURCE=200809L
CFLAGS_RELEASE = -Wall -Wextra -std=c23 -pedantic -O2 -MMD -MP -D_POSIX_C_SOURCE=200809L

SRC_DIR = src
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release
TEST_DIR = test

SRC = $(wildcard $(SRC_DIR)/*.c)
DEBUG_OBJ = $(patsubst $(SRC_DIR)/%.c, $(DEBUG_DIR)/%.o, $(SRC))
RELEASE_OBJ = $(patsubst $(SRC_DIR)/%.c, $(RELEASE_DIR)/%.o, $(SRC))

DEBUG_TARGET = $(DEBUG_DIR)/dirwalk
RELEASE_TARGET = $(RELEASE_DIR)/dirwalk

all: release

$(BUILD_DIR) $(DEBUG_DIR) $(RELEASE_DIR):
	mkdir -p $@

$(DEBUG_TARGET): $(DEBUG_OBJ) | $(DEBUG_DIR)
	$(CC) $(CFLAGS_DEBUG) $(DEBUG_OBJ) -o $@

$(RELEASE_TARGET): $(RELEASE_OBJ) | $(RELEASE_DIR)
	$(CC) $(CFLAGS_RELEASE) $(RELEASE_OBJ) -o $@

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_DIR)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_DIR)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

DEP = $(DEBUG_OBJ:.o=.d) $(RELEASE_OBJ:.o=.d)
-include $(DEP)

clean:
	rm -f $(DEBUG_DIR)/*.o $(DEBUG_DIR)/*.d $(DEBUG_TARGET)
	rm -f $(RELEASE_DIR)/*.o $(RELEASE_DIR)/*.d $(RELEASE_TARGET)
	rm -f $(TEST_DIR)/*

test: debug release
	mkdir -p $(TEST_DIR)
	ln -sf ../$(DEBUG_TARGET) $(TEST_DIR)/dirwalk-debug
	ln -sf ../$(RELEASE_TARGET) $(TEST_DIR)/dirwalk-release

memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(DEBUG_TARGET)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(DEBUG_TARGET)

release: CFLAGS = $(CFLAGS_RELEASE)
release: $(RELEASE_TARGET)

run-debug: debug
	$(DEBUG_TARGET)

run-release: release
	$(RELEASE_TARGET)

.PHONY: all debug release clean test memcheck run-debug run-release
