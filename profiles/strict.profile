# ─────────────────────────────────────────────────────────────
# DevilShell — Strict Sandbox Profile
# ─────────────────────────────────────────────────────────────
# Maximum isolation: full namespace separation + strict
# syscall allowlisting. Only essential + general syscalls pass.
# ─────────────────────────────────────────────────────────────

# Namespace isolation — all enabled
namespace_pid=true         # Isolated PID tree
namespace_mount=true       # Isolated filesystem view
namespace_network=true     # No network access
namespace_user=true        # User namespace (no root needed)
namespace_uts=true         # Isolated hostname

# Seccomp filtering — strict allowlist
seccomp_enabled=true
seccomp_mode=strict        # Only allow essential syscalls
seccomp_log=true           # Log blocked syscalls for debugging

# Environment
hostname=dsh-strict
