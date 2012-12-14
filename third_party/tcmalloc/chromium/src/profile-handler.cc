// Copyright (c) 2009, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat
//         Nabeel Mian
//
// Implements management of profile timers and the corresponding signal handler.

#include "config.h"
#include "profile-handler.h"

#if !(defined(__CYGWIN__) || defined(__CYGWIN32__))

#include <errno.h>
#include <stdio.h>
#include <sys/time.h>

#include <list>
#include <map>
#include <string>

#include "base/dynamic_annotations.h"
#include "base/googleinit.h"
#include "base/logging.h"
#include "base/spinlock.h"
#include "maybe_threads.h"

using std::list;
using std::map;
using std::string;

// This structure is used by ProfileHandlerRegisterCallback and
// ProfileHandlerUnregisterCallback as a handle to a registered callback.
struct ProfileHandlerToken {
  // Sets the callback and associated arg.
  ProfileHandlerToken(ProfileHandlerCallback cb, void* cb_arg)
      : callback(cb),
        callback_arg(cb_arg) {
  }

  // Callback function to be invoked on receiving a profile timer interrupt.
  ProfileHandlerCallback callback;
  // Argument for the callback function.
  void* callback_arg;
};

// Blocks a signal from being delivered to the current thread while the object
// is alive. Restores previous state upon destruction.
class ScopedSignalBlocker {
 public:
  ScopedSignalBlocker(int signo) {
    sigemptyset(&sig_set_);
    sigaddset(&sig_set_, signo);
    RAW_CHECK(sigprocmask(SIG_BLOCK, &sig_set_, NULL) == 0,
              "sigprocmask (block)");
  }
  ~ScopedSignalBlocker() {
    RAW_CHECK(sigprocmask(SIG_UNBLOCK, &sig_set_, NULL) == 0,
              "sigprocmask (unblock)");
  }

 private:
  sigset_t sig_set_;
};

// This class manages profile timers and associated signal handler. This is a
// a singleton.
class ProfileHandler {
 public:
  // Registers the current thread with the profile handler.
  void RegisterThread();

  // Unregisters the current thread with the profile handler.
  void UnregisterThread();

  // Registers a callback routine to receive profile timer ticks. The returned
  // token is to be used when unregistering this callback and must not be
  // deleted by the caller.
  ProfileHandlerToken* RegisterCallback(ProfileHandlerCallback callback,
                                        void* callback_arg);

  // Unregisters a previously registered callback. Expects the token returned
  // by the corresponding RegisterCallback routine.
  void UnregisterCallback(ProfileHandlerToken* token)
      NO_THREAD_SAFETY_ANALYSIS;

  // Unregisters all the callbacks and stops the timer(s).
  void Reset();

  // Gets the current state of profile handler.
  void GetState(ProfileHandlerState* state);

  // Initializes and returns the ProfileHandler singleton.
  static ProfileHandler* Instance();

 private:
  ProfileHandler();
  ~ProfileHandler();

  // Largest allowed frequency.
  static const int32 kMaxFrequency = 4000;
  // Default frequency.
  static const int32 kDefaultFrequency = 100;

  // ProfileHandler singleton.
  static ProfileHandler* instance_;

  // pthread_once_t for one time initialization of ProfileHandler singleton.
  static pthread_once_t once_;

#if defined(HAVE_TLS)
  // Timer state as configured previously. This bit is kept in
  // |registered_threads_| if TLS is not available.
  static __thread bool timer_running_;
#endif

  // Initializes the ProfileHandler singleton via GoogleOnceInit.
  static void Init();

  // The number of SIGPROF (or SIGALRM for ITIMER_REAL) interrupts received.
  int64 interrupts_ GUARDED_BY(signal_lock_);

  // SIGPROF/SIGALRM interrupt frequency, read-only after construction.
  int32 frequency_;

  // ITIMER_PROF (which uses SIGPROF), or ITIMER_REAL (which uses SIGALRM)
  int timer_type_;

  // Counts the number of callbacks registered.
  int32 callback_count_ GUARDED_BY(control_lock_);

  // Is profiling allowed at all?
  bool allowed_;

  // This lock serializes the registration of threads and protects the
  // callbacks_ list below.
  // Locking order:
  // In the context of a signal handler, acquire signal_lock_ to walk the
  // callback list. Otherwise, acquire control_lock_, disable the signal
  // handler and then acquire signal_lock_.
  SpinLock control_lock_ ACQUIRED_BEFORE(signal_lock_);
  SpinLock signal_lock_;

  // Set of threads registered through RegisterThread. This is consulted
  // whenever timers are switched on or off to post a signal to these threads
  // so they can arm their timers. Locking rules as for |callbacks_| below. If
  // TLS is not available, this map is also used to keep the thread's timer
  // state.
  map<pthread_t, bool> registered_threads_ GUARDED_BY(signal_lock_);

  // Holds the list of registered callbacks. We expect the list to be pretty
  // small. Currently, the cpu profiler (base/profiler) and thread module
  // (base/thread.h) are the only two components registering callbacks.
  // Following are the locking requirements for callbacks_:
  // For read-write access outside the SIGPROF handler:
  //  - Acquire control_lock_
  //  - Disable SIGPROF handler.
  //  - Acquire signal_lock_
  // For read-only access in the context of SIGPROF handler
  // (Read-write access is *not allowed* in the SIGPROF handler)
  //  - Acquire signal_lock_
  // For read-only access outside SIGPROF handler:
  //  - Acquire control_lock_
  typedef list<ProfileHandlerToken*> CallbackList;
  typedef CallbackList::iterator CallbackIterator;
  CallbackList callbacks_ GUARDED_BY(signal_lock_);

  // Returns the signal to be used with |timer_type_| (SIGPROF or SIGALARM).
  int signal_number() {
    return (timer_type_ == ITIMER_PROF ? SIGPROF : SIGALRM);
  }

  // Starts or stops the interval timer(s). In case of non-shared timers, this
  // will post a signal to all registered threads, causing them to arm their
  // timers.
  void UpdateTimers(bool enable) EXCLUSIVE_LOCKS_REQUIRED(signal_lock_);

  // Configures the timer. If |frequency| is 0, the timer will be disabled.
  void SetupTimer(int32 frequency) EXCLUSIVE_LOCKS_REQUIRED(signal_lock_);

  // Returns true if the handler is not being used by something else.
  // This checks the kernel's signal handler table.
  bool IsSignalHandlerAvailable();

  // SIGPROF/SIGALRM handler. Iterate over and call all the registered callbacks.
  static void SignalHandler(int sig, siginfo_t* sinfo, void* ucontext);

  DISALLOW_COPY_AND_ASSIGN(ProfileHandler);
};

ProfileHandler* ProfileHandler::instance_ = NULL;
pthread_once_t ProfileHandler::once_ = PTHREAD_ONCE_INIT;

#if defined(HAVE_TLS)
bool __thread ProfileHandler::timer_running_ = false;
#endif

const int32 ProfileHandler::kMaxFrequency;
const int32 ProfileHandler::kDefaultFrequency;

// If we are LD_PRELOAD-ed against a non-pthreads app, then pthread_* functions
// won't be defined.  We declare them here, for that case (with weak linkage)
// which will cause the non-definition to resolve to NULL.  We can then check
// for NULL or not in Instance.
extern "C" int pthread_once(pthread_once_t *, void (*)(void))
    ATTRIBUTE_WEAK;
extern "C" int pthread_kill(pthread_t thread_id, int signo)
    ATTRIBUTE_WEAK;

void ProfileHandler::Init() {
  instance_ = new ProfileHandler();
}

ProfileHandler* ProfileHandler::Instance() {
  if (pthread_once) {
    pthread_once(&once_, Init);
  }
  if (instance_ == NULL) {
    // This will be true on systems that don't link in pthreads,
    // including on FreeBSD where pthread_once has a non-zero address
    // (but doesn't do anything) even when pthreads isn't linked in.
    Init();
    assert(instance_ != NULL);
  }
  return instance_;
}

ProfileHandler::ProfileHandler()
    : interrupts_(0),
      callback_count_(0),
      allowed_(true) {
  SpinLockHolder cl(&control_lock_);

  timer_type_ = (getenv("CPUPROFILE_REALTIME") ? ITIMER_REAL : ITIMER_PROF);

  // Get frequency of interrupts (if specified)
  char junk;
  const char* fr = getenv("CPUPROFILE_FREQUENCY");
  if (fr != NULL && (sscanf(fr, "%u%c", &frequency_, &junk) == 1) &&
      (frequency_ > 0)) {
    // Limit to kMaxFrequency
    frequency_ = (frequency_ > kMaxFrequency) ? kMaxFrequency : frequency_;
  } else {
    frequency_ = kDefaultFrequency;
  }

  if (!allowed_) {
    return;
  }

  // If something else is using the signal handler,
  // assume it has priority over us and stop.
  if (!IsSignalHandlerAvailable()) {
    RAW_LOG(INFO, "Disabling profiler because %s handler is already in use.",
            signal_number());
    allowed_ = false;
    return;
  }

  // Install the signal handler.
  struct sigaction sa;
  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  RAW_CHECK(sigaction(signal_number(), &sa, NULL) == 0, "sigprof (enable)");
}

ProfileHandler::~ProfileHandler() {
  Reset();
}

void ProfileHandler::RegisterThread() {
  SpinLockHolder cl(&control_lock_);

  if (!allowed_) {
    return;
  }

  // Record the thread identifier and start the timer if profiling is on.
  ScopedSignalBlocker block(signal_number());
  SpinLockHolder sl(&signal_lock_);
  registered_threads_[pthread_self()] = false;
  SetupTimer(callback_count_ > 0 ? frequency_ : 0);
}

void ProfileHandler::UnregisterThread() {
  ScopedSignalBlocker block(signal_number());
  SpinLockHolder sl(&signal_lock_);
  registered_threads_.erase(pthread_self());
}

ProfileHandlerToken* ProfileHandler::RegisterCallback(
    ProfileHandlerCallback callback, void* callback_arg) {

  ProfileHandlerToken* token = new ProfileHandlerToken(callback, callback_arg);

  SpinLockHolder cl(&control_lock_);
  {
    ScopedSignalBlocker block(signal_number());
    SpinLockHolder sl(&signal_lock_);
    callbacks_.push_back(token);
    ++callback_count_;
    UpdateTimers(true);
  }
  return token;
}

void ProfileHandler::UnregisterCallback(ProfileHandlerToken* token) {
  SpinLockHolder cl(&control_lock_);
  for (CallbackIterator it = callbacks_.begin(); it != callbacks_.end();
       ++it) {
    if ((*it) == token) {
      RAW_CHECK(callback_count_ > 0, "Invalid callback count");
      {
        ScopedSignalBlocker block(signal_number());
        SpinLockHolder sl(&signal_lock_);
        delete *it;
        callbacks_.erase(it);
        --callback_count_;
        if (callback_count_ == 0)
          UpdateTimers(false);
      }
      return;
    }
  }
  // Unknown token.
  RAW_LOG(FATAL, "Invalid token");
}

void ProfileHandler::Reset() {
  SpinLockHolder cl(&control_lock_);
  {
    ScopedSignalBlocker block(signal_number());
    SpinLockHolder sl(&signal_lock_);
    CallbackIterator it = callbacks_.begin();
    while (it != callbacks_.end()) {
      CallbackIterator tmp = it;
      ++it;
      delete *tmp;
      callbacks_.erase(tmp);
    }
    callback_count_ = 0;
    UpdateTimers(false);
  }
}

void ProfileHandler::GetState(ProfileHandlerState* state) {
  SpinLockHolder cl(&control_lock_);
  {
    ScopedSignalBlocker block(signal_number());
    SpinLockHolder sl(&signal_lock_);  // Protects interrupts_.
    state->interrupts = interrupts_;
  }
  state->frequency = frequency_;
  state->callback_count = callback_count_;
  state->allowed = allowed_;
}

void ProfileHandler::UpdateTimers(bool enabled) {
  if (!allowed_) {
    return;
  }
  SetupTimer(enabled ? frequency_ : 0);

  // Tell other threads to start their timers.
  for (map<pthread_t, bool>::const_iterator thread(registered_threads_.begin());
       thread != registered_threads_.end();
       ++thread) {
    if (thread->first != pthread_self() && pthread_kill) {
      int err = pthread_kill(thread->first, signal_number());
      if (err == ESRCH) {
        // Thread exited.
        registered_threads_.erase(thread->first);
      } else {
        RAW_CHECK(err == 0, "pthread_kill");
      }
    }
  }
}

void ProfileHandler::SetupTimer(int32 frequency) {
  bool enable = (frequency > 0);
#if defined(HAVE_TLS)
  bool& timer_running = timer_running_;
#else
  bool& timer_running = registered_threads_[pthread_self()];
#endif
  if (enable == timer_running)
    return;
  struct itimerval timer;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = (enable ? (1000000 / frequency) : 0);
  timer.it_value = timer.it_interval;
  setitimer(timer_type_, &timer, 0);
  timer_running = enable;
}

bool ProfileHandler::IsSignalHandlerAvailable() {
  struct sigaction sa;
  RAW_CHECK(sigaction(signal_number(), NULL, &sa) == 0,
            "is-signal-handler avail");

  // We only take over the handler if the current one is unset.
  // It must be SIG_IGN or SIG_DFL, not some other function.
  // SIG_IGN must be allowed because when profiling is allowed but
  // not actively in use, this code keeps the handler set to SIG_IGN.
  // That setting will be inherited across fork+exec.  In order for
  // any child to be able to use profiling, SIG_IGN must be treated
  // as available.
  return sa.sa_handler == SIG_IGN || sa.sa_handler == SIG_DFL;
}

void ProfileHandler::SignalHandler(int sig, siginfo_t* sinfo, void* ucontext) {
  int saved_errno = errno;
  // At this moment, instance_ must be initialized because the handler is
  // enabled in RegisterThread or RegisterCallback only after
  // ProfileHandler::Instance runs.
  ProfileHandler* instance = ANNOTATE_UNPROTECTED_READ(instance_);
  RAW_CHECK(instance != NULL, "ProfileHandler is not initialized");
  {
    SpinLockHolder sl(&instance->signal_lock_);
    ++instance->interrupts_;
    // Enable/Disable the timer if necessary.
    instance->SetupTimer(
        instance->callbacks_.empty() ? 0 : instance->frequency_);
    for (CallbackIterator it = instance->callbacks_.begin();
         it != instance->callbacks_.end();
         ++it) {
      (*it)->callback(sig, sinfo, ucontext, (*it)->callback_arg);
    }
  }
  errno = saved_errno;
}

// This module initializer registers the main thread, so it must be
// executed in the context of the main thread.
REGISTER_MODULE_INITIALIZER(profile_main, ProfileHandlerRegisterThread());

extern "C" void ProfileHandlerRegisterThread() {
  ProfileHandler::Instance()->RegisterThread();
}

extern "C" void ProfileHandlerUnregisterThread() {
  ProfileHandler::Instance()->UnregisterThread();
}

extern "C" ProfileHandlerToken* ProfileHandlerRegisterCallback(
    ProfileHandlerCallback callback, void* callback_arg) {
  return ProfileHandler::Instance()->RegisterCallback(callback, callback_arg);
}

extern "C" void ProfileHandlerUnregisterCallback(ProfileHandlerToken* token) {
  ProfileHandler::Instance()->UnregisterCallback(token);
}

extern "C" void ProfileHandlerReset() {
  return ProfileHandler::Instance()->Reset();
}

extern "C" void ProfileHandlerGetState(ProfileHandlerState* state) {
  ProfileHandler::Instance()->GetState(state);
}

#else  // OS_CYGWIN

// ITIMER_PROF doesn't work under cygwin.  ITIMER_REAL is available, but doesn't
// work as well for profiling, and also interferes with alarm().  Because of
// these issues, unless a specific need is identified, profiler support is
// disabled under Cygwin.
extern "C" void ProfileHandlerRegisterThread() {
}

extern "C" void ProfileHandlerUnregisterThread() {
}

extern "C" ProfileHandlerToken* ProfileHandlerRegisterCallback(
    ProfileHandlerCallback callback, void* callback_arg) {
  return NULL;
}

extern "C" void ProfileHandlerUnregisterCallback(ProfileHandlerToken* token) {
}

extern "C" void ProfileHandlerReset() {
}

extern "C" void ProfileHandlerGetState(ProfileHandlerState* state) {
}

#endif  // OS_CYGWIN
