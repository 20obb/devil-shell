# DevilShell (dsh)

> A custom Linux shell built from scratch in **C** + **Rust**, focused on **speed**, **stability**, and **sandboxing**.

```
     ██████╗ ███████╗██╗   ██╗██╗██╗     
     ██╔══██╗██╔════╝██║   ██║██║██║     
     ██║  ██║█████╗  ██║   ██║██║██║     
     ██║  ██║██╔══╝  ╚██╗ ██╔╝██║██║     
     ██████╔╝███████╗ ╚████╔╝ ██║███████╗
     ╚═════╝ ╚══════╝  ╚═══╝  ╚═╝╚══════╝
             ╔═══════════════╗
             ║   S H E L L   ║
             ╚═══════════════╝
```

## Features

- **Custom Shell Engine** — full REPL with command parsing, pipes, I/O redirection, background jobs
- **Built-in Line Editor** — cursor movement, history (↑↓), tab completion, Ctrl shortcuts (no readline dependency)
- **Sandbox Mode** — run commands inside Linux namespace isolation (PID, mount, network, user, UTS)
- **Seccomp Filtering** — restrict system calls with BPF filters (permissive/default/strict profiles)
- **Variable Expansion** — `$VAR`, `${VAR}`, `~` expansion
- **Script Mode** — execute `.dsh` scripts or pipe commands via stdin
- **Colored Prompt** — two-line prompt with sandbox indicator

## Architecture

| Layer | Language | Files |
|-------|----------|-------|
| Shell Core (REPL, parser, executor) | C | `src/main.c`, `parser.c`, `executor.c`, ... |
| Sandbox Engine (namespaces, seccomp) | Rust | `sandbox-rs/src/lib.rs`, `namespace.rs`, `seccomp.rs` |
| Profile System | Rust + Config | `sandbox-rs/src/profile.rs` + `profiles/*.profile` |

## Build

```bash
# Prerequisites: gcc, cargo (Rust)
make
```

## Usage

```bash
# Interactive mode
./dsh

# Script mode
./dsh script.dsh

# Inside dsh:
help                    # Show all commands
sandbox default         # Enter sandbox mode (permissive)
sandbox strict          # Enter sandbox mode (strict isolation)
sandbox network         # Enter sandbox mode (no network)
sandbox status          # Show sandbox configuration
sandbox exit            # Exit sandbox mode
```

## Sandbox Profiles

| Profile | Namespaces | Seccomp | Description |
|---------|-----------|---------|-------------|
| `default` | user, UTS | permissive (log) | Safe starting point, logs dangerous syscalls |
| `strict` | all (PID, mount, net, user, UTS) | strict (allowlist) | Maximum isolation |
| `network` | net, user, UTS | block network | Prevents network access |

## Built-in Commands

`cd`, `exit`, `help`, `history`, `export`, `unset`, `env`, `sandbox`

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| ↑/↓ | History navigation |
| Tab | File path completion |
| Ctrl+C | Cancel current line |
| Ctrl+D | Exit (empty line) / Delete char |
| Ctrl+A/E | Move to start/end of line |
| Ctrl+K/U | Kill to end/start of line |
| Ctrl+W | Delete word backward |
| Ctrl+L | Clear screen |

## License

GPL 2.0V
