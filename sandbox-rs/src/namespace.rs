//! Namespace isolation — apply Linux namespaces via unshare().
//!
//! Called in a child process after fork(), before exec().
//! The parent shell process is NEVER sandboxed.

use crate::profile::SandboxProfile;

/// Apply namespace isolation based on the loaded profile.
///
/// This calls unshare(2) with the appropriate flags to create new
/// namespaces for the current process. Must be called after fork()
/// in the child process.
pub fn apply_namespaces(prof: &SandboxProfile) -> Result<(), String> {
    let mut flags: libc::c_int = 0;

    // User namespace should come first — it allows unprivileged creation
    // of other namespace types on Linux >= 3.8
    if prof.ns_user {
        flags |= libc::CLONE_NEWUSER;
    }

    if prof.ns_pid {
        flags |= libc::CLONE_NEWPID;
    }

    if prof.ns_mount {
        flags |= libc::CLONE_NEWNS;
    }

    if prof.ns_network {
        flags |= libc::CLONE_NEWNET;
    }

    if prof.ns_uts {
        flags |= libc::CLONE_NEWUTS;
    }

    if flags == 0 {
        // No namespaces requested
        return Ok(());
    }

    // Attempt unshare with all flags at once
    let ret = unsafe { libc::unshare(flags) };
    if ret != 0 {
        let errno = unsafe { *libc::__errno_location() };

        // If combined unshare fails, try individual namespaces
        // to give a better error message
        if flags != libc::CLONE_NEWUSER {
            return try_individual_namespaces(prof, errno);
        }

        return Err(format!(
            "unshare(0x{:x}) failed: {} (errno {})",
            flags,
            errno_to_string(errno),
            errno
        ));
    }

    // After creating a user namespace, set up UID/GID mapping
    if prof.ns_user {
        setup_uid_mapping()?;
    }

    // If UTS namespace was created, set the hostname
    if prof.ns_uts {
        set_sandbox_hostname(&prof.hostname)?;
    }

    Ok(())
}

/// Try each namespace individually to identify which one fails
fn try_individual_namespaces(prof: &SandboxProfile, orig_errno: i32) -> Result<(), String> {
    let namespaces = [
        (prof.ns_user, libc::CLONE_NEWUSER, "user"),
        (prof.ns_pid, libc::CLONE_NEWPID, "PID"),
        (prof.ns_mount, libc::CLONE_NEWNS, "mount"),
        (prof.ns_network, libc::CLONE_NEWNET, "network"),
        (prof.ns_uts, libc::CLONE_NEWUTS, "UTS"),
    ];

    // Try user namespace first (it enables the others)
    for &(enabled, flag, name) in &namespaces {
        if !enabled {
            continue;
        }
        let ret = unsafe { libc::unshare(flag) };
        if ret != 0 {
            let errno = unsafe { *libc::__errno_location() };
            eprintln!(
                "[dsh sandbox] warning: {} namespace failed: {} (errno {})",
                name,
                errno_to_string(errno),
                errno
            );
            // For user namespace, this is fatal
            if flag == libc::CLONE_NEWUSER {
                return Err(format!(
                    "user namespace unavailable: {} — try: sysctl kernel.unprivileged_userns_clone=1",
                    errno_to_string(orig_errno)
                ));
            }
            // For others, continue (best-effort isolation)
        }
    }

    // Set up UID mapping if user namespace succeeded
    if prof.ns_user {
        let _ = setup_uid_mapping();
    }

    if prof.ns_uts {
        let _ = set_sandbox_hostname(&prof.hostname);
    }

    Ok(())
}

/// Set up UID/GID mapping for user namespace.
///
/// Maps the current UID/GID inside the namespace to the same
/// UID/GID outside, so file access works normally.
fn setup_uid_mapping() -> Result<(), String> {
    let uid = unsafe { libc::getuid() };
    let gid = unsafe { libc::getgid() };

    // Write UID map: <inside_uid> <outside_uid> <count>
    let uid_map = format!("0 {} 1\n", uid);
    if std::fs::write("/proc/self/uid_map", &uid_map).is_err() {
        // Not fatal — some kernels restrict this
        eprintln!("[dsh sandbox] warning: could not write uid_map");
    }

    // Must deny setgroups before writing gid_map (Linux security requirement)
    if std::fs::write("/proc/self/setgroups", "deny\n").is_err() {
        eprintln!("[dsh sandbox] warning: could not write setgroups");
    }

    // Write GID map
    let gid_map = format!("0 {} 1\n", gid);
    if std::fs::write("/proc/self/gid_map", &gid_map).is_err() {
        eprintln!("[dsh sandbox] warning: could not write gid_map");
    }

    Ok(())
}

/// Set hostname inside the UTS namespace
fn set_sandbox_hostname(hostname: &str) -> Result<(), String> {
    let ret = unsafe {
        libc::sethostname(hostname.as_ptr() as *const libc::c_char, hostname.len())
    };
    if ret != 0 {
        // Not fatal
        eprintln!("[dsh sandbox] warning: could not set hostname");
    }
    Ok(())
}

/// Convert errno to human-readable string
fn errno_to_string(errno: i32) -> &'static str {
    match errno {
        libc::EPERM => "Operation not permitted",
        libc::EINVAL => "Invalid argument",
        libc::ENOMEM => "Out of memory",
        libc::ENOSPC => "No space left (max namespaces reached)",
        libc::EUSERS => "Max user namespaces exceeded",
        libc::ENOSYS => "System call not supported",
        _ => "Unknown error",
    }
}
