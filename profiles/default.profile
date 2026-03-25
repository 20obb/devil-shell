# ─────────────────────────────────────────────────────────────
# DevilShell — Default Sandbox Profile
# ─────────────────────────────────────────────────────────────
# Permissive mode: logs dangerous syscalls but doesn't block.
# Ideal for learning what your commands actually do.
# ─────────────────────────────────────────────────────────────

# Namespace isolation
namespace_pid=false        # Don't isolate PID (keep simple for default)
namespace_mount=false      # Don't isolate mounts
namespace_network=false    # Allow network access
namespace_user=true        # User namespace (no root needed)
namespace_uts=true         # Isolate hostname

# Seccomp filtering
seccomp_enabled=true
seccomp_mode=permissive    # Log dangerous syscalls, don't block
seccomp_log=true           # Enable logging of flagged calls

# Environment
hostname=dsh-sandbox
