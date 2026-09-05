#ifndef CZG3_AUTO_SIGRETURN_H
#define CZG3_AUTO_SIGRETURN_H

#include <stdint.h>

#if defined(APP_CZG3_AUTOROOT_SIGRETURN) && APP_CZG3_AUTOROOT_SIGRETURN

int czg3_auto_sigreturn_enabled(void);
int czg3_auto_sigreturn_proxy_active_for_tid(int tid);
int czg3_auto_sigreturn_frame_ready_for_tid(int tid);
void czg3_auto_sigreturn_sched_started(int tid);
void czg3_auto_sigreturn_sched_complete(int tid, long ret, int saved_errno);

long czg3_auto_pselect_dispatch(long number, long nfds, long readfds,
                                long writefds, long exceptfds,
                                long timeout, long sigmask);

#else

static inline int czg3_auto_sigreturn_enabled(void) { return 0; }
static inline int czg3_auto_sigreturn_proxy_active_for_tid(int tid) {
  (void)tid;
  return 0;
}
static inline int czg3_auto_sigreturn_frame_ready_for_tid(int tid) {
  (void)tid;
  return 0;
}
static inline void czg3_auto_sigreturn_sched_started(int tid) { (void)tid; }
static inline void czg3_auto_sigreturn_sched_complete(int tid, long ret,
                                                       int saved_errno) {
  (void)tid;
  (void)ret;
  (void)saved_errno;
}

#endif

#endif
