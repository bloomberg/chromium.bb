// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/callbacks.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"

namespace webkit {
namespace ppapi {

// CallbackTracker -------------------------------------------------------------

CallbackTracker::CallbackTracker() {
}

void CallbackTracker::AbortAll() {
  // Iterate over a copy since |Abort()| calls |Remove()| (indirectly).
  // TODO(viettrungluu): This obviously isn't so efficient.
  CallbackSetMap pending_callbacks_copy = pending_callbacks_;
  for (CallbackSetMap::iterator it1 = pending_callbacks_copy.begin();
       it1 != pending_callbacks_copy.end(); ++it1) {
    for (CallbackSet::iterator it2 = it1->second.begin();
         it2 != it1->second.end(); ++it2) {
      (*it2)->Abort();
    }
  }
}

void CallbackTracker::PostAbortForResource(PP_Resource resource_id) {
  CHECK(resource_id != 0);
  CallbackSetMap::iterator it1 = pending_callbacks_.find(resource_id);
  if (it1 == pending_callbacks_.end())
    return;
  for (CallbackSet::iterator it2 = it1->second.begin();
       it2 != it1->second.end(); ++it2) {
    // Post the abort.
    (*it2)->PostAbort();
  }
}

CallbackTracker::~CallbackTracker() {
  // All callbacks must be aborted before destruction.
  CHECK_EQ(0u, pending_callbacks_.size());
}

void CallbackTracker::Add(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  PP_Resource resource_id = tracked_callback->resource_id();
  DCHECK(pending_callbacks_[resource_id].find(tracked_callback) ==
         pending_callbacks_[resource_id].end());
  pending_callbacks_[resource_id].insert(tracked_callback);
}

void CallbackTracker::Remove(
    const scoped_refptr<TrackedCallback>& tracked_callback) {
  CallbackSetMap::iterator map_it =
      pending_callbacks_.find(tracked_callback->resource_id());
  DCHECK(map_it != pending_callbacks_.end());
  CallbackSet::iterator it = map_it->second.find(tracked_callback);
  DCHECK(it != map_it->second.end());
  map_it->second.erase(it);

  // If there are no pending callbacks left for this ID, get rid of the entry.
  if (map_it->second.empty())
    pending_callbacks_.erase(map_it);
}

// TrackedCallback -------------------------------------------------------------

TrackedCallback::TrackedCallback(const scoped_refptr<CallbackTracker>& tracker,
                                 PP_Resource resource_id)
    : ALLOW_THIS_IN_INITIALIZER_LIST(abort_impl_factory_(this)),
      tracker_(tracker),
      resource_id_(resource_id),
      completed_(false),
      aborted_(false) {
  tracker_->Add(make_scoped_refptr(this));
}

void TrackedCallback::Abort() {
  if (!completed()) {
    aborted_ = true;
    AbortImpl();
  }
}

void TrackedCallback::PostAbort() {
  if (!completed()) {
    aborted_ = true;
    // Post a task for the abort (only if necessary).
    if (abort_impl_factory_.empty()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          abort_impl_factory_.NewRunnableMethod(&TrackedCallback::AbortImpl));
    }
  }
}

TrackedCallback::~TrackedCallback() {
  // The tracker must ensure that the callback is completed (maybe abortively).
  DCHECK(completed_);
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

// TrackedCompletionCallback ---------------------------------------------------

TrackedCompletionCallback::TrackedCompletionCallback(
    const scoped_refptr<CallbackTracker>& tracker,
    PP_Resource resource_id,
    const PP_CompletionCallback& callback)
    : TrackedCallback(tracker, resource_id),
      callback_(callback) {
}

void TrackedCompletionCallback::Run(int32_t result) {
  if (!completed()) {
    // Cancel any pending calls.
    abort_impl_factory_.RevokeAll();

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

void TrackedCompletionCallback::AbortImpl() {
  Run(PP_ERROR_ABORTED);
}

}  // namespace ppapi
}  // namespace webkit
