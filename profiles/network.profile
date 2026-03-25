# ─────────────────────────────────────────────────────────────
# DevilShell — Network Isolation Profile
# ─────────────────────────────────────────────────────────────
# Blocks all network syscalls (socket, connect, bind, etc.)
# while allowing everything else. Use this when you want to
# run untrusted commands that shouldn't phone home.
# ─────────────────────────────────────────────────────────────

# Namespace isolation
namespace_pid=false        # Keep PID visible
namespace_mount=false      # Keep filesystem normal
namespace_network=true     # Isolate network stack
namespace_user=true        # User namespace (no root needed)
namespace_uts=true         # Isolate hostname

# Seccomp filtering — block network syscalls
seccomp_enabled=true
seccomp_mode=default       # Block dangerous + network syscalls
seccomp_log=true           # Log blocked syscalls

# Environment
hostname=dsh-offline
