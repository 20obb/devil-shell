//! Seccomp BPF filter — restrict syscalls in sandboxed processes.
//!
//! Builds and applies a BPF filter program that enforces an allow-list
//! or block-list of system calls depending on the profile's seccomp mode.
//!
//! Architecture: x86_64 only (audit arch check included in the filter).

use crate::profile::{SandboxProfile, SeccompMode};

// ── BPF Constants ────────────────────────────────────────────────

// BPF instruction classes
const BPF_LD: u16 = 0x00;
const BPF_JMP: u16 = 0x05;
const BPF_RET: u16 = 0x06;

// BPF operand sizes
const BPF_W: u16 = 0x00;

// BPF source operands  
const BPF_ABS: u16 = 0x20;
const BPF_K: u16 = 0x00;

// BPF jump conditions
const BPF_JEQ: u16 = 0x10;
const BPF_JGE: u16 = 0x30;

// Seccomp return actions
const SECCOMP_RET_ALLOW: u32 = 0x7fff_0000;
const SECCOMP_RET_KILL_PROCESS: u32 = 0x8000_0000;
const SECCOMP_RET_LOG: u32 = 0x7ffc_0000;
const SECCOMP_RET_ERRNO: u32 = 0x0005_0000;

// Seccomp data offsets (struct seccomp_data)
const OFFSET_NR: u32 = 0;       // syscall number
const OFFSET_ARCH: u32 = 4;     // architecture

// x86_64 audit architecture
const AUDIT_ARCH_X86_64: u32 = 0xC000_003E;

// Prctl constants
const PR_SET_NO_NEW_PRIVS: libc::c_int = 38;
const PR_SET_SECCOMP: libc::c_int = 22;
const SECCOMP_MODE_FILTER: libc::c_ulong = 2;

// ── BPF Instruction ──────────────────────────────────────────────

#[repr(C)]
#[derive(Clone, Copy)]
struct SockFilter {
    code: u16,
    jt: u8,
    jf: u8,
    k: u32,
}

#[repr(C)]
struct SockFprog {
    len: u16,
    filter: *const SockFilter,
}

/// Create a BPF_STMT instruction
fn bpf_stmt(code: u16, k: u32) -> SockFilter {
    SockFilter {
        code,
        jt: 0,
        jf: 0,
        k,
    }
}

/// Create a BPF_JUMP instruction
fn bpf_jump(code: u16, k: u32, jt: u8, jf: u8) -> SockFilter {
    SockFilter { code, jt, jf, k }
}

// ── Syscall Lists ────────────────────────────────────────────────

/// Essential syscalls needed for any process to function
const ESSENTIAL_SYSCALLS: &[u32] = &[
    0,   // read
    1,   // write
    3,   // close
    9,   // mmap
    10,  // mprotect
    11,  // munmap
    12,  // brk
    13,  // rt_sigaction
    14,  // rt_sigprocmask
    15,  // rt_sigreturn
    21,  // access
    24,  // sched_yield
    35,  // nanosleep
    39,  // getpid
    60,  // exit
    63,  // uname
    158, // arch_prctl
    218, // set_tid_address
    228, // clock_gettime
    231, // exit_group
    234, // tgkill
    257, // openat
    262, // newfstatat
    302, // prlimit64
    318, // getrandom
    334, // rseq
];

/// Syscalls needed for general command execution (fork, exec, file ops)
const GENERAL_SYSCALLS: &[u32] = &[
    2,   // open
    4,   // stat
    5,   // fstat
    6,   // lstat
    7,   // poll
    8,   // lseek
    16,  // ioctl
    17,  // pread64
    18,  // pwrite64
    19,  // readv
    20,  // writev
    22,  // pipe
    23,  // select
    25,  // mremap
    28,  // madvise
    32,  // dup
    33,  // dup2
    34,  // pause
    37,  // alarm
    38,  // setitimer
    41,  // socket (controlled separately)
    56,  // clone
    57,  // fork
    58,  // vfork
    59,  // execve
    61,  // wait4
    62,  // kill
    72,  // fcntl
    73,  // flock
    74,  // fsync
    75,  // fdatasync
    76,  // truncate
    77,  // ftruncate
    78,  // getdents
    79,  // getcwd
    80,  // chdir
    82,  // rename
    83,  // mkdir
    84,  // rmdir
    85,  // creat
    86,  // link
    87,  // unlink
    88,  // symlink
    89,  // readlink
    90,  // chmod
    91,  // fchmod
    92,  // chown
    95,  // umask
    96,  // gettimeofday
    97,  // getrlimit
    99,  // sysinfo
    102, // getuid
    104, // getgid
    107, // geteuid
    108, // getegid
    110, // getppid
    111, // getpgrp
    112, // setsid
    116, // setgroups
    131, // sigaltstack
    137, // statfs
    138, // fstatfs
    157, // prctl
    160, // setrlimit
    186, // gettid
    200, // tkill
    202, // futex
    204, // sched_getaffinity
    217, // getdents64
    233, // epoll_ctl
    270, // pselect6
    273, // set_robust_list
    281, // epoll_pwait
    284, // eventfd
    288, // accept4
    290, // epoll_create1
    291, // dup3
    292, // pipe2
    309, // getcpu
    322, // execveat
    332, // statx
    435, // clone3
    439, // faccessat2
];

/// Network-related syscalls (blocked in network profile)
const NETWORK_SYSCALLS: &[u32] = &[
    41,  // socket
    42,  // connect
    43,  // accept
    44,  // sendto
    45,  // recvfrom
    46,  // sendmsg
    47,  // recvmsg
    48,  // shutdown
    49,  // bind
    50,  // listen
    51,  // getsockname
    52,  // getpeername
    53,  // socketpair
    54,  // setsockopt
    55,  // getsockopt
    288, // accept4
];

/// Dangerous syscalls (always blocked in default/strict)
const DANGEROUS_SYSCALLS: &[u32] = &[
    26,  // ptrace
    101, // ptrace (old)
    103, // syslog
    130, // init_module
    165, // mount
    166, // umount2
    167, // swapon
    168, // swapoff
    169, // reboot
    170, // sethostname (outside sandbox)
    171, // setdomainname
    175, // init_module
    176, // delete_module
    246, // kexec_load
    247, // waitid (used by some exploits)
    310, // process_vm_readv
    311, // process_vm_writev
    314, // sched_setattr
    320, // kexec_file_load
];

// ── Build BPF Filter ─────────────────────────────────────────────

/// Build a BPF filter for the given seccomp mode
fn build_filter(mode: &SeccompMode, log_blocked: bool) -> Vec<SockFilter> {
    let mut filter = Vec::new();

    // Step 1: Verify architecture is x86_64
    // Load architecture from seccomp_data
    filter.push(bpf_stmt(BPF_LD | BPF_W | BPF_ABS, OFFSET_ARCH));
    // If not x86_64, kill the process
    filter.push(bpf_jump(
        BPF_JMP | BPF_JEQ | BPF_K,
        AUDIT_ARCH_X86_64,
        1, // true: skip next instruction (continue)
        0, // false: fall through to kill
    ));
    filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));

    // Step 2: Load syscall number
    filter.push(bpf_stmt(BPF_LD | BPF_W | BPF_ABS, OFFSET_NR));

    // Step 3: Apply rules based on mode
    let default_action = if log_blocked {
        SECCOMP_RET_LOG
    } else {
        SECCOMP_RET_ERRNO | 1 // EPERM
    };

    match mode {
        SeccompMode::Permissive => {
            // Permissive: allow everything but log dangerous syscalls
            for &nr in DANGEROUS_SYSCALLS {
                filter.push(bpf_jump(BPF_JMP | BPF_JEQ | BPF_K, nr, 0, 1));
                filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_LOG));
            }
            // Default: allow
            filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
        }

        SeccompMode::Default => {
            // Default: block dangerous syscalls, allow the rest
            for &nr in DANGEROUS_SYSCALLS {
                filter.push(bpf_jump(BPF_JMP | BPF_JEQ | BPF_K, nr, 0, 1));
                filter.push(bpf_stmt(BPF_RET | BPF_K, default_action));
            }
            // Default: allow
            filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
        }

        SeccompMode::Strict => {
            // Strict: allow-list mode
            // Build jump table for allowed syscalls
            let allowed: Vec<u32> = {
                let mut v: Vec<u32> = Vec::new();
                v.extend_from_slice(ESSENTIAL_SYSCALLS);
                v.extend_from_slice(GENERAL_SYSCALLS);
                v.sort();
                v.dedup();
                v
            };

            // For each allowed syscall: if match → allow
            let num_allowed = allowed.len();
            for (i, &nr) in allowed.iter().enumerate() {
                let remaining = num_allowed - i - 1;
                filter.push(bpf_jump(
                    BPF_JMP | BPF_JEQ | BPF_K,
                    nr,
                    (remaining * 2 + 1) as u8, // jump to ALLOW
                    0,                           // check next
                ));
            }

            // If no match: deny
            filter.push(bpf_stmt(BPF_RET | BPF_K, default_action));
            // ALLOW target
            filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
        }
    }

    filter
}

/// Build a filter that blocks network syscalls
fn build_network_deny_filter(log_blocked: bool) -> Vec<SockFilter> {
    let mut filter = Vec::new();

    // Verify architecture
    filter.push(bpf_stmt(BPF_LD | BPF_W | BPF_ABS, OFFSET_ARCH));
    filter.push(bpf_jump(
        BPF_JMP | BPF_JEQ | BPF_K,
        AUDIT_ARCH_X86_64,
        1,
        0,
    ));
    filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));

    // Load syscall number
    filter.push(bpf_stmt(BPF_LD | BPF_W | BPF_ABS, OFFSET_NR));

    let deny_action = if log_blocked {
        SECCOMP_RET_LOG
    } else {
        SECCOMP_RET_ERRNO | 1
    };

    // Block network syscalls
    for &nr in NETWORK_SYSCALLS {
        filter.push(bpf_jump(BPF_JMP | BPF_JEQ | BPF_K, nr, 0, 1));
        filter.push(bpf_stmt(BPF_RET | BPF_K, deny_action));
    }

    // Also block dangerous syscalls
    for &nr in DANGEROUS_SYSCALLS {
        filter.push(bpf_jump(BPF_JMP | BPF_JEQ | BPF_K, nr, 0, 1));
        filter.push(bpf_stmt(BPF_RET | BPF_K, deny_action));
    }

    // Allow everything else
    filter.push(bpf_stmt(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));

    filter
}

// ── Apply Seccomp Filter ─────────────────────────────────────────

/// Apply a seccomp BPF filter to the current process.
/// Must be called after fork(), in the child process, before exec().
pub fn apply_seccomp_filter(prof: &SandboxProfile) -> Result<(), String> {
    // Step 1: Set PR_SET_NO_NEW_PRIVS (required before seccomp)
    let ret = unsafe { libc::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) };
    if ret != 0 {
        return Err(format!(
            "prctl(NO_NEW_PRIVS) failed: errno {}",
            unsafe { *libc::__errno_location() }
        ));
    }

    // Step 2: Build the appropriate filter
    let filter = if prof.name == "network" {
        build_network_deny_filter(prof.seccomp_log)
    } else {
        build_filter(&prof.seccomp_mode, prof.seccomp_log)
    };

    if filter.is_empty() {
        return Ok(());
    }

    // Validate filter length
    if filter.len() > u16::MAX as usize {
        return Err("BPF filter too large".to_string());
    }

    // Step 3: Apply the filter
    let prog = SockFprog {
        len: filter.len() as u16,
        filter: filter.as_ptr(),
    };

    let ret = unsafe {
        libc::prctl(
            PR_SET_SECCOMP,
            SECCOMP_MODE_FILTER as libc::c_ulong,
            &prog as *const SockFprog as libc::c_ulong,
            0,
            0,
        )
    };

    if ret != 0 {
        let errno = unsafe { *libc::__errno_location() };
        return Err(format!(
            "seccomp(FILTER) failed: errno {} — kernel may lack CONFIG_SECCOMP_FILTER",
            errno
        ));
    }

    Ok(())
}
