# ─────────────────────────────────────────────────────────────────
# DevilShell (dsh) — Makefile
# Build system: Rust static lib + C compilation + final linking
# ─────────────────────────────────────────────────────────────────

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -std=gnu11 -I./include
LDFLAGS  = -lpthread -ldl -lm

# Binary name
TARGET   = dsh

# Source directories
SRC_DIR  = src
INC_DIR  = include
RUST_DIR = sandbox-rs

# C sources and objects
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(SRCS:.c=.o)

# Rust static library
RUST_LIB = $(RUST_DIR)/target/release/libdsh_sandbox.a

# ── Default Target ────────────────────────────────────────────

.PHONY: all clean install uninstall rust-lib

all: rust-lib $(TARGET)
	@echo ""
	@echo "\033[1;92m  ✓ Build complete: ./$(TARGET)\033[0m"
	@echo "\033[2m    Run with: ./dsh\033[0m"
	@echo ""

# ── Rust Sandbox Library ──────────────────────────────────────

rust-lib:
	@echo "\033[1;96m  → Building Rust sandbox engine...\033[0m"
	@cd $(RUST_DIR) && $(HOME)/.cargo/bin/cargo build --release 2>&1 | \
		sed 's/^/    /'
	@echo "\033[1;92m  ✓ Rust library built\033[0m"

# ── C Compilation ─────────────────────────────────────────────

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(wildcard $(INC_DIR)/*.h)
	@echo "\033[2m  CC  $<\033[0m"
	@$(CC) $(CFLAGS) -c $< -o $@

# ── Final Linking ─────────────────────────────────────────────

$(TARGET): $(OBJS) $(RUST_LIB)
	@echo "\033[1;96m  → Linking $(TARGET)...\033[0m"
	@$(CC) $(OBJS) -L$(RUST_DIR)/target/release -ldsh_sandbox \
		$(LDFLAGS) -o $(TARGET)
	@echo "\033[1;92m  ✓ Linked: $(TARGET)\033[0m"

# ── Install / Uninstall ──────────────────────────────────────

install: $(TARGET)
	@echo "\033[1;96m  → Installing to /usr/local/bin/$(TARGET)\033[0m"
	@install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@mkdir -p /etc/dsh/profiles
	@cp -r profiles/* /etc/dsh/profiles/ 2>/dev/null || true
	@echo "\033[1;92m  ✓ Installed\033[0m"

uninstall:
	@rm -f /usr/local/bin/$(TARGET)
	@rm -rf /etc/dsh
	@echo "\033[1;93m  ✓ Uninstalled\033[0m"

# ── Clean ─────────────────────────────────────────────────────

clean:
	@echo "\033[2m  Cleaning...\033[0m"
	@rm -f $(OBJS) $(TARGET)
	@cd $(RUST_DIR) && $(HOME)/.cargo/bin/cargo clean 2>/dev/null || true
	@echo "\033[1;93m  ✓ Clean\033[0m"
