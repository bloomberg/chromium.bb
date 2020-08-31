#ifndef EV_API_H
#define EV_API_H

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifndef pTHX_
# define pTHX_
# define aTHX_
# define pTHX
# define aTHX
#endif

#define EV_COMMON				\
  int e_flags; /* cheap on 64 bit systems */	\
  SV *loop;                                     \
  SV *self;    /* contains this struct */	\
  SV *cb_sv, *fh, *data;

#ifndef EV_PROTOTYPES
# define EV_PROTOTYPES 0
#endif

#ifndef EV_H
# define EV_H <EV/ev.h>
#endif

#include EV_H

struct EVAPI {
  I32 ver;
  I32 rev;
#define EV_API_VERSION 5
#define EV_API_REVISION 1

  struct ev_loop *default_loop;
  unsigned int supported_backends;
  unsigned int recommended_backends;
  unsigned int embeddable_backends;

  /* TODO: remove on major API bump */
  /* perl fh or fd int to fd */
  int (*sv_fileno) (SV *fh);
  /* signal number/name to signum */
  int (*sv_signum) (SV *fh);

  /* same as libev functions */
  ev_tstamp (*time_)(void);
  void (*sleep_)(ev_tstamp);

  struct ev_loop *(*loop_new)(unsigned int);
  void (*loop_destroy)(EV_P);
  void (*loop_fork)(EV_P);
  unsigned int (*backend)(EV_P);
  unsigned int (*iteration)(EV_P);
  unsigned int (*depth)(EV_P);
  ev_tstamp (*now)(EV_P);
  void (*now_update)(EV_P);
  int (*run)(EV_P_ int flags);
  void (*break_)(EV_P_ int how);
  void (*suspend)(EV_P);
  void (*resume) (EV_P);
  void (*ref)  (EV_P);
  void (*unref)(EV_P);
  void (*set_userdata)(EV_P_ void *data);
  void *(*userdata)   (EV_P);
  void (*set_loop_release_cb)  (EV_P_ void (*release)(EV_P), void (*acquire)(EV_P));
  void (*set_invoke_pending_cb)(EV_P_ void (*invoke_pending_cb)(EV_P));
  unsigned int (*pending_count)(EV_P);
  void (*invoke_pending)       (EV_P);
  void (*verify)               (EV_P);

  void (*once)(EV_P_ int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg);

  void (*invoke)(EV_P_ void *, int);
  int  (*clear_pending)(EV_P_ void *);

  void (*io_start)(EV_P_ ev_io *);
  void (*io_stop) (EV_P_ ev_io *);
  void (*timer_start)(EV_P_ ev_timer *);
  void (*timer_stop) (EV_P_ ev_timer *);
  void (*timer_again)(EV_P_ ev_timer *);
  ev_tstamp (*timer_remaining) (EV_P_ ev_timer *);
  void (*periodic_start)(EV_P_ ev_periodic *);
  void (*periodic_stop) (EV_P_ ev_periodic *);
  void (*signal_start)(EV_P_ ev_signal *);
  void (*signal_stop) (EV_P_ ev_signal *);
  void (*child_start)(EV_P_ ev_child *);
  void (*child_stop) (EV_P_ ev_child *);
  void (*stat_start)(EV_P_ ev_stat *);
  void (*stat_stop) (EV_P_ ev_stat *);
  void (*stat_stat) (EV_P_ ev_stat *);
  void (*idle_start)(EV_P_ ev_idle *);
  void (*idle_stop) (EV_P_ ev_idle *);
  void (*prepare_start)(EV_P_ ev_prepare *);
  void (*prepare_stop) (EV_P_ ev_prepare *);
  void (*check_start)(EV_P_ ev_check *);
  void (*check_stop) (EV_P_ ev_check *);
  void (*embed_start)(EV_P_ ev_embed *);
  void (*embed_stop) (EV_P_ ev_embed *);
  void (*embed_sweep)(EV_P_ ev_embed *);
  void (*fork_start) (EV_P_ ev_fork *);
  void (*fork_stop)  (EV_P_ ev_fork *);
  void (*cleanup_start) (EV_P_ ev_cleanup *);
  void (*cleanup_stop)  (EV_P_ ev_cleanup *);
  void (*async_start)(EV_P_ ev_async *);
  void (*async_stop) (EV_P_ ev_async *);
  void (*async_send) (EV_P_ ev_async *);
};

#if !EV_PROTOTYPES
# undef EV_DEFAULT
# undef EV_DEFAULT_
# undef EV_DEFAULT_UC
# undef EV_DEFAULT_UC_
# undef EV_A_
# define EV_DEFAULT                GEVAPI->default_loop
# define EV_DEFAULT_UC             GEVAPI->default_loop
# define ev_supported_backends()   GEVAPI->supported_backends
# define ev_recommended_backends() GEVAPI->recommended_backends
# define ev_embeddable_backends()  GEVAPI->embeddable_backends

# define sv_fileno(sv)             GEVAPI->sv_fileno (sv)
# define sv_signum(sv)             GEVAPI->sv_signum (sv)

# define ev_time()                 GEVAPI->time_ ()
# define ev_sleep(time)            GEVAPI->sleep_ ((time))

# define ev_loop_new(flags)        GEVAPI->loop_new ((flags))
# define ev_loop_destroy(loop)     GEVAPI->loop_destroy ((loop))
# define ev_loop_fork(loop)        GEVAPI->loop_fork ((loop))
# define ev_backend(loop)          GEVAPI->backend ((loop))
# define ev_iteration(loop)        GEVAPI->iteration ((loop))
# define ev_depth(loop)            GEVAPI->depth ((depth))
# define ev_now(loop)              GEVAPI->now ((loop))
# define ev_now_update(loop)       GEVAPI->now_update ((loop))
# define ev_run(l,flags)           GEVAPI->run ((l), (flags))
# define ev_break(loop,how)        GEVAPI->break_ ((loop), (how))
# define ev_suspend(loop)          GEVAPI->suspend ((loop))
# define ev_resume(loop)           GEVAPI->resume ((loop))
# define ev_ref(loop)              GEVAPI->ref   (loop)
# define ev_unref(loop)            GEVAPI->unref (loop)
# define ev_set_userdata(l,p)      GEVAPI->set_userdata ((l), (p))
# define ev_userdata(l)            GEVAPI->userdata (l)
# define ev_set_loop_release_cb(l,r,a) GEVAPI->set_loop_release_cb ((l), (r), (a))
# define ev_set_invoke_pending_cb(l,c) GEVAPI->set_invoke_pending_cb ((l), (c))
# define ev_invoke_pending(l)      GEVAPI->invoke_pending ((l))
# define ev_pending_count(l)       GEVAPI->pending_count ((l))
# define ev_verify(l)              GEVAPI->verify ((l))

# define ev_once(loop,fd,events,timeout,cb,arg) GEVAPI->once ((loop), (fd), (events), (timeout), (cb), (arg))

# define ev_invoke(l,w,rev)        GEVAPI->invoke ((l), (w), (rev))
# define ev_clear_pending(l,w)     GEVAPI->clear_pending ((l), (w))
# define ev_io_start(l,w)          GEVAPI->io_start ((l), (w))
# define ev_io_stop(l,w)           GEVAPI->io_stop  ((l), (w))
# define ev_timer_start(l,w)       GEVAPI->timer_start ((l), (w))
# define ev_timer_stop(l,w)        GEVAPI->timer_stop  ((l), (w))
# define ev_timer_again(l,w)       GEVAPI->timer_again ((l), (w))
# define ev_timer_remaining(l,w)   GEVAPI->timer_remaining ((l), (w))
# define ev_periodic_start(l,w)    GEVAPI->periodic_start ((l), (w))
# define ev_periodic_stop(l,w)     GEVAPI->periodic_stop  ((l), (w))
# define ev_signal_start(l,w)      GEVAPI->signal_start ((l), (w))
# define ev_signal_stop(l,w)       GEVAPI->signal_stop  ((l), (w))
# define ev_idle_start(l,w)        GEVAPI->idle_start ((l), (w))
# define ev_idle_stop(l,w)         GEVAPI->idle_stop  ((l), (w))
# define ev_prepare_start(l,w)     GEVAPI->prepare_start ((l), (w))
# define ev_prepare_stop(l,w)      GEVAPI->prepare_stop  ((l), (w))
# define ev_check_start(l,w)       GEVAPI->check_start ((l), (w))
# define ev_check_stop(l,w)        GEVAPI->check_stop  ((l), (w))
# define ev_child_start(l,w)       GEVAPI->child_start ((l), (w))
# define ev_child_stop(l,w)        GEVAPI->child_stop  ((l), (w))
# define ev_stat_start(l,w)        GEVAPI->stat_start ((l), (w))
# define ev_stat_stop(l,w)         GEVAPI->stat_stop  ((l), (w))
# define ev_stat_stat(l,w)         GEVAPI->stat_stat  ((l), (w))
# define ev_embed_start(l,w)       GEVAPI->embed_start ((l), (w))
# define ev_embed_stop(l,w)        GEVAPI->embed_stop  ((l), (w))
# define ev_embed_sweep(l,w)       GEVAPI->embed_sweep ((l), (w))
# define ev_fork_start(l,w)        GEVAPI->fork_start ((l), (w))
# define ev_fork_stop(l,w)         GEVAPI->fork_stop  ((l), (w))
# define ev_cleanup_start(l,w)     GEVAPI->cleanup_start ((l), (w))
# define ev_cleanup_stop(l,w)      GEVAPI->cleanup_stop  ((l), (w))
# define ev_async_start(l,w)       GEVAPI->async_start ((l), (w))
# define ev_async_stop(l,w)        GEVAPI->async_stop  ((l), (w))
# define ev_async_send(l,w)        GEVAPI->async_send  ((l), (w))
#endif

static struct EVAPI *GEVAPI;

#define I_EV_API(YourName)                                                                 \
STMT_START {                                                                               \
  SV *sv = perl_get_sv ("EV::API", 0);                                                     \
  if (!sv) croak ("EV::API not found");                                                    \
  GEVAPI = (struct EVAPI*) SvIV (sv);                                                      \
  if (GEVAPI->ver != EV_API_VERSION                                                        \
      || GEVAPI->rev < EV_API_REVISION)                                                    \
    croak ("EV::API version mismatch (%d.%d vs. %d.%d) -- please recompile '%s'",          \
           (int)GEVAPI->ver, (int)GEVAPI->rev, EV_API_VERSION, EV_API_REVISION, YourName); \
} STMT_END

#endif

