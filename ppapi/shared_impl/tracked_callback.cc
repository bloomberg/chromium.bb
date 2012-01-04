// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/tracked_callback.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

// TrackedCallback -------------------------------------------------------------

// Note: don't keep a Resource* since it may go out of scope before us.
TrackedCallback::TrackedCallback(
    Resource* resource,
    const PP_CompletionCallback& callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(abort_impl_factory_(this)),
      resource_id_(resource->pp_resource()),
      completed_(false),
      aborted_(false),
      callback_(callback) {
  tracker_ = PpapiGlobals::Get()->GetCallbackTrackerForInstance(
      resource->pp_instance()),
  tracker_->Add(make_scoped_refptr(this));
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
  if (!completed()) {
    aborted_ = true;
    // Post a task for the abort (only if necessary).
    if (!abort_impl_factory_.HasWeakPtrs()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TrackedCallback::Abort,
                     abort_impl_factory_.GetWeakPtr()));
    }
  }
}

void TrackedCallback::Run(int32_t result) {
  if (!completed()) {
    // Cancel any pending calls.
    abort_impl_factory_.InvalidateWeakPtrs();

    // Copy |callback_| and look at |aborted()| now, since |MarkAsCompleted()|
    // may delete us.
    PP_CompletionCallback callback = callback_;
    if (aborted())
      result = PP_ERROR_ABORTED;

    // Do this before running the callback in case of reentrancy (which
    // shouldn't happen, but avoid strange failures).
    MarkAsCompleted();
    PP_RunCompletionCallback(&callback, result);
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

void TrackedCallback::MarkAsCompleted() {
  DCHECK(!completed());

  // We will be removed; maintain a reference to ensure we won't be deleted
  // until we're done.
  scoped_refptr<TrackedCallback> thiz = this;
  completed_ = true;
  tracker_->Remove(thiz);
  tracker_ = NULL;
}

}  // namespace ppapi
