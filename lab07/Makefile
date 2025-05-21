CC := gcc
CFLAGS   := -Wall -Wextra -std=c11 -pedantic -MMD -MP \
            -g -Wno-unused-parameter -Wno-unused-variable \
            -D_POSIX_C_SOURCE=200809L

SRC_DIR := src
BUILD_DIR := build
DEBUG_DIR := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(SRCS))
OBJS_RELEASE := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_DIR)/%.o,$(SRCS))
TARGET_DEBUG := $(DEBUG_DIR)/app
TARGET_RELEASE := $(RELEASE_DIR)/app

.PHONY: all debug release clean
all: debug

debug: $(DEBUG_DIR) $(TARGET_DEBUG)
	@echo "Сборка debug завершена"

release: $(RELEASE_DIR) $(TARGET_RELEASE)
	@echo "Сборка release завершена"

# Правила сборки
$(DEBUG_DIR):
	mkdir -p $(DEBUG_DIR)

$(RELEASE_DIR):
	mkdir -p $(RELEASE_DIR)

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_DEBUG): $(OBJS_DEBUG)
	$(CC) $^ -o $@

$(TARGET_RELEASE): $(OBJS_RELEASE)
	$(CC) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "Очищено"
