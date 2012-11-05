// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/tracked_callback.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "ppapi/c/dev/ppb_message_loop_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

// TrackedCallback -------------------------------------------------------------

// Note: don't keep a Resource* since it may go out of scope before us.
TrackedCallback::TrackedCallback(
    Resource* resource,
    const PP_CompletionCallback& callback)
    : is_scheduled_(false),
      resource_id_(resource ? resource->pp_resource() : 0),
      completed_(false),
      aborted_(false),
      callback_(callback),
      result_for_blocked_callback_(PP_OK) {
  // TODO(dmichael): Add tracking at the instance level, for callbacks that only
  // have an instance (e.g. for MouseLock).
  if (resource) {
    tracker_ = PpapiGlobals::Get()->GetCallbackTrackerForInstance(
        resource->pp_instance());
    tracker_->Add(make_scoped_refptr(this));
  }

  base::Lock* proxy_lock = PpapiGlobals::Get()->GetProxyLock();
  // We only need a ConditionVariable if the lock is valid (i.e., we're out-of-
  // process) and the callback is blocking.
  if (proxy_lock && is_blocking())
    operation_completed_condvar_.reset(new base::ConditionVariable(proxy_lock));
}

TrackedCallback::~TrackedCallback() {
}

void TrackedCallback::Abort() {
  // It doesn't make sense to abort a callback that's not associated with a
  // resource.
  DCHECK(resource_id_);
  Run(PP_ERROR_ABORTED);
}

void TrackedCallback::PostAbort() {
  PostRun(PP_ERROR_ABORTED);
}

void TrackedCallback::Run(int32_t result) {
  // Only allow the callback to be run once. Note that this also covers the case
  // where the callback was previously Aborted because its associated Resource
  // went away. The callback may live on for a while because of a reference from
  // a Closure. But when the Closure runs, Run() quietly does nothing, and the
  // callback will go away when all referring Closures go away.
  if (completed())
    return;
  if (result == PP_ERROR_ABORTED)
    aborted_ = true;

  // Copy |callback_| and look at |aborted()| now, since |MarkAsCompleted()|
  // may delete us.
  PP_CompletionCallback callback = callback_;
  // Note that this call of Run() may have been scheduled prior to Abort() or
  // PostAbort() being called. If we have been told to Abort, that always
  // trumps a result that was scheduled before.
  if (aborted())
    result = PP_ERROR_ABORTED;

  if (is_blocking()) {
    // If the condition variable is invalid, there are two possibilities. One,
    // we're running in-process, in which case the call should have come in on
    // the main thread and we should have returned PP_ERROR_BLOCKS_MAIN_THREAD
    // well before this. Otherwise, this callback was not created as a
    // blocking callback. Either way, there's some internal error.
    if (!operation_completed_condvar_.get()) {
      NOTREACHED();
      return;
    }
    result_for_blocked_callback_ = result;
    // Retain ourselves, since MarkAsCompleted will remove us from the
    // tracker. Then MarkAsCompleted before waking up the blocked thread,
    // which could potentially re-enter.
    scoped_refptr<TrackedCallback> thiz(this);
    MarkAsCompleted();
    // Wake up the blocked thread. See BlockUntilComplete for where the thread
    // Wait()s.
    operation_completed_condvar_->Signal();
  } else {
    // Do this before running the callback in case of reentrancy (which
    // shouldn't happen, but avoid strange failures).
    MarkAsCompleted();
    // TODO(dmichael): Associate a message loop with the callback; if it's not
    // the same as the current thread's loop, then post it to the right loop.
    CallWhileUnlocked(PP_RunCompletionCallback, &callback, result);
  }
}

void TrackedCallback::PostRun(int32_t result) {
  if (completed()) {
    NOTREACHED();
    return;
  }
  if (result == PP_ERROR_ABORTED)
    aborted_ = true;
  // We might abort when there's already a scheduled callback, but callers
  // should never try to PostRun more than once otherwise.
  DCHECK(result == PP_ERROR_ABORTED || !is_scheduled_);

  base::Closure callback_closure(
      RunWhileLocked(base::Bind(&TrackedCallback::Run, this, result)));
  MessageLoop::current()->PostTask(FROM_HERE, callback_closure);
  is_scheduled_ = true;
}

// static
bool TrackedCallback::IsPending(
    const scoped_refptr<TrackedCallback>& callback) {
  if (!callback.get())
    return false;
  return !callback->completed();
}

int32_t TrackedCallback::BlockUntilComplete() {
  // Note, we are already holding the proxy lock in all these methods, including
  // this one (see ppapi/thunk/enter.cc for where it gets acquired).

  // It doesn't make sense to wait on a non-blocking callback. Furthermore,
  // BlockUntilComplete should never be called for in-process plugins, where
  // blocking callbacks are not supported.
  CHECK(operation_completed_condvar_.get());
  if (!is_blocking() || !operation_completed_condvar_.get()) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  while (!completed())
    operation_completed_condvar_->Wait();
  return result_for_blocked_callback_;
}

void TrackedCallback::MarkAsCompleted() {
  DCHECK(!completed());

  // We will be removed; maintain a reference to ensure we won't be deleted
  // until we're done.
  scoped_refptr<TrackedCallback> thiz = this;
  completed_ = true;
  // We may not have a valid resource, in which case we're not in the tracker.
  if (resource_id_)
    tracker_->Remove(thiz);
  tracker_ = NULL;
}

}  // namespace ppapi
