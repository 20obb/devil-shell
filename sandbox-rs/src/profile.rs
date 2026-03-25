//! Profile parser — reads .profile config files.
//!
//! Format (simple key=value):
//! ```text
//! # Comment
//! namespace_pid=true
//! namespace_mount=false
//! namespace_network=false
//! namespace_user=true
//! namespace_uts=true
//! seccomp_enabled=true
//! seccomp_mode=permissive    # permissive | default | strict
//! seccomp_log=true
//! hostname=dsh-sandbox
//! ```

use std::collections::HashMap;
use std::fs;

/// Seccomp enforcement mode
#[derive(Debug, Clone, PartialEq)]
pub enum SeccompMode {
    /// Log only — don't kill, just report
    Permissive,
    /// Block dangerous syscalls, allow most others
    Default,
    /// Allow only essential syscalls
    Strict,
}

/// Parsed sandbox profile
#[derive(Debug, Clone)]
pub struct SandboxProfile {
    pub name: String,

    // Namespace flags
    pub ns_pid: bool,
    pub ns_mount: bool,
    pub ns_network: bool,
    pub ns_user: bool,
    pub ns_uts: bool,

    // Seccomp settings
    pub seccomp_enabled: bool,
    pub seccomp_mode: SeccompMode,
    pub seccomp_log: bool,

    // Sandbox environment
    pub hostname: String,
}

impl Default for SandboxProfile {
    fn default() -> Self {
        SandboxProfile {
            name: String::from("default"),
            ns_pid: false,
            ns_mount: false,
            ns_network: false,
            ns_user: true,  // user namespace doesn't need root
            ns_uts: true,
            seccomp_enabled: true,
            seccomp_mode: SeccompMode::Permissive,
            seccomp_log: true,
            hostname: String::from("dsh-sandbox"),
        }
    }
}

impl SandboxProfile {
    /// Generate a human-readable status string
    pub fn status_string(&self) -> String {
        let mode_str = match self.seccomp_mode {
            SeccompMode::Permissive => "permissive (log only)",
            SeccompMode::Default => "default (block dangerous)",
            SeccompMode::Strict => "strict (allowlist only)",
        };

        format!(
            "\x1b[1;92m  Sandbox: active\x1b[0m\n\
             \x1b[2m  ─────────────────────────\x1b[0m\n\
               Profile:    \x1b[93m{}\x1b[0m\n\
               Namespaces:\n\
                 PID:       {}\n\
                 Mount:     {}\n\
                 Network:   {}\n\
                 User:      {}\n\
                 UTS:       {}\n\
               Seccomp:    {}\n\
               Mode:       {}\n\
               Logging:    {}\n\
               Hostname:   {}\n",
            self.name,
            yn(self.ns_pid),
            yn(self.ns_mount),
            yn(self.ns_network),
            yn(self.ns_user),
            yn(self.ns_uts),
            yn(self.seccomp_enabled),
            mode_str,
            yn(self.seccomp_log),
            self.hostname,
        )
    }
}

fn yn(b: bool) -> &'static str {
    if b {
        "\x1b[92m✓\x1b[0m"
    } else {
        "\x1b[91m✗\x1b[0m"
    }
}

/// Parse a profile from a file path
pub fn parse_profile(path: &str) -> Result<SandboxProfile, String> {
    let content =
        fs::read_to_string(path).map_err(|e| format!("cannot read '{}': {}", path, e))?;

    let mut prof = SandboxProfile::default();

    // Extract filename as profile name
    if let Some(fname) = path.rsplit('/').next() {
        if let Some(name) = fname.strip_suffix(".profile") {
            prof.name = name.to_string();
        }
    }

    let kv: HashMap<String, String> = content
        .lines()
        .filter(|line| {
            let trimmed = line.trim();
            !trimmed.is_empty() && !trimmed.starts_with('#')
        })
        .filter_map(|line| {
            let mut parts = line.splitn(2, '=');
            let key = parts.next()?.trim().to_lowercase();
            let val = parts.next()?.trim().to_string();
            // Strip inline comments
            let val = val.split('#').next().unwrap_or("").trim().to_string();
            Some((key, val))
        })
        .collect();

    // Parse namespace flags
    if let Some(v) = kv.get("namespace_pid") {
        prof.ns_pid = parse_bool(v);
    }
    if let Some(v) = kv.get("namespace_mount") {
        prof.ns_mount = parse_bool(v);
    }
    if let Some(v) = kv.get("namespace_network") {
        prof.ns_network = parse_bool(v);
    }
    if let Some(v) = kv.get("namespace_user") {
        prof.ns_user = parse_bool(v);
    }
    if let Some(v) = kv.get("namespace_uts") {
        prof.ns_uts = parse_bool(v);
    }

    // Parse seccomp settings
    if let Some(v) = kv.get("seccomp_enabled") {
        prof.seccomp_enabled = parse_bool(v);
    }
    if let Some(v) = kv.get("seccomp_mode") {
        prof.seccomp_mode = match v.to_lowercase().as_str() {
            "permissive" | "log" => SeccompMode::Permissive,
            "default" | "normal" => SeccompMode::Default,
            "strict" | "paranoid" => SeccompMode::Strict,
            _ => SeccompMode::Permissive,
        };
    }
    if let Some(v) = kv.get("seccomp_log") {
        prof.seccomp_log = parse_bool(v);
    }

    // Parse environment
    if let Some(v) = kv.get("hostname") {
        prof.hostname = v.clone();
    }

    Ok(prof)
}

fn parse_bool(s: &str) -> bool {
    matches!(
        s.to_lowercase().as_str(),
        "true" | "yes" | "1" | "on" | "enabled"
    )
}
