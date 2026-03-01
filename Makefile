CC ?= cc
CSTD ?= -std=c99
CFLAGS ?= -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=

BIN_DIR := bin
OBJ_DIR := build

COMMON_SRCS := \
	src/common/log.c \
	src/common/signal.c \
	src/common/socket.c \
	src/common/mdns.c \
	src/common/config.c \
	src/common/table.c

SERVER_SRCS := src/server/main.c
CLIENT_SRCS := src/client/main.c
BROWSER_SRCS := src/browser/main.c

SERVER_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRCS) $(SERVER_SRCS))
CLIENT_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRCS) $(CLIENT_SRCS))
BROWSER_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRCS) $(BROWSER_SRCS))

TARGETS := $(BIN_DIR)/umdns_server $(BIN_DIR)/umdns_client $(BIN_DIR)/umdns_browser

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

REMOTE_HOST ?=
REMOTE_PATH ?= ~/.local/bin

.PHONY: all clean install install_remote smoke

all: $(TARGETS)

$(BIN_DIR)/umdns_server: $(SERVER_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(BIN_DIR)/umdns_client: $(CLIENT_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(BIN_DIR)/umdns_browser: $(BROWSER_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: all
	install -d $(BINDIR)
	install -m 0755 $(BIN_DIR)/umdns_server $(BINDIR)/umdns_server
	install -m 0755 $(BIN_DIR)/umdns_client $(BINDIR)/umdns_client
	install -m 0755 $(BIN_DIR)/umdns_browser $(BINDIR)/umdns_browser

install_remote: all
	@test -n "$(REMOTE_HOST)" || (echo "REMOTE_HOST is required" && exit 1)
	ssh $(REMOTE_HOST) "mkdir -p $(REMOTE_PATH)"
	scp $(BIN_DIR)/umdns_server $(BIN_DIR)/umdns_client $(BIN_DIR)/umdns_browser $(REMOTE_HOST):$(REMOTE_PATH)/

smoke: all
	./scripts/smoke/smoke.sh
