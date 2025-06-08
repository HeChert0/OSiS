CC := gcc
CFLAGS   := -Wall -Wextra -std=c11 -pedantic -MMD -MP \
            -g -Wno-unused-parameter -Wno-unused-variable \
            -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
            -Wno-format-truncation
LDFLAGS  := -pthread

SRC_DIR     := src
BUILD_DIR   := build
DEBUG_DIR   := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

SERVER_BIN := server
CLIENT_BIN := client

SERVER_SRC := $(SRC_DIR)/server.c
CLIENT_SRC := $(SRC_DIR)/client.c
COMMON_SRC := $(SRC_DIR)/common.h

SERVER_OBJS_DBG := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(SERVER_SRC) $(COMMON_SRC))
CLIENT_OBJS_DBG := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(CLIENT_SRC) $(COMMON_SRC))

SERVER_OBJS_REL := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_DIR)/%.o,$(SERVER_SRC) $(COMMON_SRC))
CLIENT_OBJS_REL := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_DIR)/%.o,$(CLIENT_SRC) $(COMMON_SRC))

.PHONY: all debug release clean help
all: debug

debug: CFLAGS += -DDEBUG
debug: prepare_dirs $(DEBUG_DIR)/$(SERVER_BIN) $(DEBUG_DIR)/$(CLIENT_BIN)

release: CFLAGS := -O2
release: prepare_dirs $(RELEASE_DIR)/$(SERVER_BIN) $(RELEASE_DIR)/$(CLIENT_BIN)

$(DEBUG_DIR)/$(SERVER_BIN): $(SERVER_OBJS_DBG)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(DEBUG_DIR)/$(CLIENT_BIN): $(CLIENT_OBJS_DBG)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(RELEASE_DIR)/$(SERVER_BIN): $(SERVER_OBJS_REL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(RELEASE_DIR)/$(CLIENT_BIN): $(CLIENT_OBJS_REL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

prepare_dirs:
	@mkdir -p $(DEBUG_DIR) $(RELEASE_DIR)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Available targets:"
	@echo "  all      - default (debug) build"
	@echo "  debug    - build server and client in debug mode"
	@echo "  release  - build server and client in release mode"
	@echo "  clean    - remove build artifacts"
