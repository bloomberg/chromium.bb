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
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      resource_id_(resource ? resource->pp_resource() : 0),
      completed_(false),
      aborted_(false),
      callback_(callback),
      result_for_blocked_callback_(PP_OK) {
  // We can only track this callback if the resource is valid. It can be NULL
  // in error conditions in the Enter* classes and for callbacks associated with
  // an instance.
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
  if (!completed()) {
    aborted_ = true;
    Run(PP_ERROR_ABORTED);
  }
}

void TrackedCallback::PostAbort() {
  // It doesn't make sense to abort a callback that's not associated with a
  // resource.
  // TODO(dmichael): If we allow associating with an instance instead, we must
  // allow for aborts in the case of the instance being destroyed.
  DCHECK(resource_id_);

  if (!completed() && !aborted_) {
    aborted_ = true;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        RunWhileLocked(base::Bind(&TrackedCallback::Abort,
                                  weak_ptr_factory_.GetWeakPtr())));
  }
}

void TrackedCallback::Run(int32_t result) {
  if (!completed()) {
    // Cancel any pending calls.
    weak_ptr_factory_.InvalidateWeakPtrs();

    // Copy |callback_| and look at |aborted()| now, since |MarkAsCompleted()|
    // may delete us.
    PP_CompletionCallback callback = callback_;
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
}

void TrackedCallback::PostRun(int32_t result) {
  DCHECK(!completed());
  if (!completed()) {
    // There should be no pending calls.
    DCHECK(!weak_ptr_factory_.HasWeakPtrs());
    weak_ptr_factory_.InvalidateWeakPtrs();

    if (resource_id_) {
      // If it has a resource_id_, it's in the tracker, and may be aborted if
      // the resource is destroyed.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          RunWhileLocked(base::Bind(&TrackedCallback::Run,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    result)));
    } else {
      // There is no resource_id_ associated with this callback, so it can't be
      // aborted. We have the message loop retain the callback to make sure it
      // gets run. This can happen when EnterBase is given an invalid resource,
      // and in that case no resource or instance will retain this
      // TrackedCallback.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          RunWhileLocked(base::Bind(&TrackedCallback::Run,
                                    this,
                                    result)));
    }
  }
}

// static
bool TrackedCallback::IsPending(
    const scoped_refptr<TrackedCallback>& callback) {
  if (!callback.get())
    return false;
  return !callback->completed();
}

// static
void TrackedCallback::ClearAndRun(scoped_refptr<TrackedCallback>* callback,
                                  int32_t result) {
  scoped_refptr<TrackedCallback> temp;
  temp.swap(*callback);
  temp->Run(result);
}

// static
void TrackedCallback::ClearAndAbort(scoped_refptr<TrackedCallback>* callback) {
  scoped_refptr<TrackedCallback> temp;
  temp.swap(*callback);
  temp->Abort();
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
