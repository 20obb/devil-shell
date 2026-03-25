//! DevilShell Sandbox Engine — Rust FFI library
//!
//! Provides namespace isolation (via unshare) and seccomp BPF filtering.
//! Compiled as a static library and linked into the C shell binary.

mod namespace;
mod profile;
mod seccomp;

use std::ffi::CStr;
use std::os::raw::{c_char, c_int};
use std::sync::Mutex;

use profile::SandboxProfile;

/// Global sandbox state (protected by mutex for thread safety)
static SANDBOX: Mutex<Option<SandboxProfile>> = Mutex::new(None);

// ── FFI Functions (called from C) ────────────────────────────────

/// Initialize the sandbox subsystem.
/// Called once at shell startup.
#[no_mangle]
pub extern "C" fn dsh_sandbox_init() -> c_int {
    // Nothing to preallocate — just verify we can lock the mutex
    match SANDBOX.lock() {
        Ok(_) => 0,
        Err(_) => -1,
    }
}

/// Load and parse a sandbox profile from a file.
/// Populates internal state with namespace and seccomp configuration.
#[no_mangle]
pub extern "C" fn dsh_sandbox_load_profile(profile_path: *const c_char) -> c_int {
    if profile_path.is_null() {
        return -1;
    }

    let path = match unsafe { CStr::from_ptr(profile_path) }.to_str() {
        Ok(s) => s,
        Err(_) => return -1,
    };

    match profile::parse_profile(path) {
        Ok(prof) => {
            if let Ok(mut guard) = SANDBOX.lock() {
                *guard = Some(prof);
                0
            } else {
                -1
            }
        }
        Err(e) => {
            eprintln!("[dsh sandbox] failed to parse profile '{}': {}", path, e);
            -1
        }
    }
}

/// Apply namespace isolation to the current process.
/// Must be called in a child process after fork(), before exec().
#[no_mangle]
pub extern "C" fn dsh_sandbox_apply_namespaces() -> c_int {
    let guard = match SANDBOX.lock() {
        Ok(g) => g,
        Err(_) => return -1,
    };

    let prof = match guard.as_ref() {
        Some(p) => p,
        None => return 0, // No profile loaded, skip
    };

    match namespace::apply_namespaces(prof) {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("[dsh sandbox] namespace error: {}", e);
            -1
        }
    }
}

/// Apply seccomp BPF filter to the current process.
/// Must be called in a child process after namespace setup, before exec().
#[no_mangle]
pub extern "C" fn dsh_sandbox_apply_seccomp() -> c_int {
    let guard = match SANDBOX.lock() {
        Ok(g) => g,
        Err(_) => return -1,
    };

    let prof = match guard.as_ref() {
        Some(p) => p,
        None => return 0,
    };

    if !prof.seccomp_enabled {
        return 0;
    }

    match seccomp::apply_seccomp_filter(prof) {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("[dsh sandbox] seccomp error: {}", e);
            -1
        }
    }
}

/// Write sandbox status information into a buffer.
#[no_mangle]
pub extern "C" fn dsh_sandbox_get_status(buf: *mut c_char, len: c_int) -> c_int {
    if buf.is_null() || len <= 0 {
        return -1;
    }

    let guard = match SANDBOX.lock() {
        Ok(g) => g,
        Err(_) => return -1,
    };

    let status = match guard.as_ref() {
        Some(prof) => prof.status_string(),
        None => String::from("  Sandbox: not configured\n"),
    };

    let bytes = status.as_bytes();
    let copy_len = std::cmp::min(bytes.len(), (len as usize) - 1);

    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), buf as *mut u8, copy_len);
        *buf.add(copy_len) = 0; // null terminate
    }

    0
}

/// Clean up sandbox subsystem.
#[no_mangle]
pub extern "C" fn dsh_sandbox_cleanup() {
    if let Ok(mut guard) = SANDBOX.lock() {
        *guard = None;
    }
}
