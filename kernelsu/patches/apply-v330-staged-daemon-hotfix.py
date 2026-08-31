#!/usr/bin/env python3
from pathlib import Path

late_load = Path("userspace/ksud/src/late_load.rs")
utils = Path("userspace/ksud/src/utils.rs")

late_text = late_load.read_text(encoding="utf-8")
old_late = '''    // Copy the daemon before loading the module changes this process's
    // security context. The remaining install steps require KernelSU policy.
    utils::stage_daemon().context("Failed to stage the running ksud")?;
'''
new_late = '''    // The app/helper pre-uploads a verified copy specifically for the privileged
    // handoff. Renaming it avoids reopening /proc/self/exe as bootstrap root,
    // which Samsung DEFEX/Safeplace can reject with EPERM before KernelSU loads.
    utils::stage_daemon_from("/data/local/tmp/.ksud-stage")
        .context("Failed to stage the pre-uploaded ksud")?;
'''
if late_text.count(old_late) != 1:
    raise SystemExit("expected v3.3.0 running-ksud staging block exactly once")
late_load.write_text(late_text.replace(old_late, new_late), encoding="utf-8")

utils_text = utils.read_text(encoding="utf-8")
old_imports = '''use rustix::fs::{Mode, OFlags, open};
use rustix::process::setpgid;
use rustix::stdio::{dup2_stderr, dup2_stdin, dup2_stdout};
'''
new_imports = '''use rustix::fs::{Mode, OFlags, chown, open};
use rustix::process::setpgid;
use rustix::stdio::{dup2_stderr, dup2_stdin, dup2_stdout};
use rustix::thread::{Gid, Uid};
'''
if utils_text.count(old_imports) != 1:
    raise SystemExit("expected v3.3.0 rustix import block exactly once")
utils_text = utils_text.replace(old_imports, new_imports)

anchor = '''pub fn finish_install(libadbroot: Option<PathBuf>, data_path: Option<PathBuf>) -> Result<()> {
'''
staged_fn = '''pub fn stage_daemon_from(staged_exe: impl AsRef<Path>) -> Result<()> {
    ensure_dir_exists(defs::ADB_DIR)?;
    let staged_exe = staged_exe.as_ref();

    if !staged_exe.is_file() {
        bail!("{} is not a staged ksud file", staged_exe.display());
    }

    // Both paths live on /data, so rename performs the handoff without opening
    // the executable for a second read. This restores the hardware-proven
    // Samsung v3.2.5 handoff and atomically replaces an older daemon.
    std::fs::rename(staged_exe, defs::DAEMON_PATH).with_context(|| {
        format!(
            "Failed to rename {} to {}",
            staged_exe.display(),
            defs::DAEMON_PATH
        )
    })?;
    chown(defs::DAEMON_PATH, Some(Uid::ROOT), Some(Gid::ROOT))?;
    #[cfg(unix)]
    set_permissions(defs::DAEMON_PATH, Permissions::from_mode(0o755))?;
    Ok(())
}

'''
if utils_text.count(anchor) != 1:
    raise SystemExit("expected v3.3.0 finish_install anchor exactly once")
if "pub fn stage_daemon_from(" in utils_text:
    raise SystemExit("stage_daemon_from already present before hotfix")
utils_text = utils_text.replace(anchor, staged_fn + anchor)
utils.write_text(utils_text, encoding="utf-8")

print("Applied staged-daemon handoff hotfix")
