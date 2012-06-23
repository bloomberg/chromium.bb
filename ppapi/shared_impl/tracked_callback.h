// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_
#define PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class CallbackTracker;
class Resource;

namespace thunk {
namespace subtle {
// For a friend declaration below.
class EnterBase;
}
}

// |TrackedCallback| represents a tracked Pepper callback (from the browser to
// the plugin), typically still pending. Such callbacks have the standard Pepper
// callback semantics. Execution (i.e., completion) of callbacks happens through
// objects of subclasses of |TrackedCallback|. Two things are ensured: (1) that
// the callback is executed at most once, and (2) once a callback is marked to
// be aborted, any subsequent completion is abortive (even if a non-abortive
// completion had previously been scheduled).
//
// The details of non-abortive completion depend on the type of callback (e.g.,
// different parameters may be required), but basic abort functionality is core.
// The ability to post aborts is needed in many situations to ensure that the
// plugin is not re-entered into. (Note that posting a task to just run
// |Abort()| is different and not correct; calling |PostAbort()| additionally
// guarantees that all subsequent completions will be abortive.)
//
// This class is reference counted so that different things can hang on to it,
// and not worry too much about ensuring Pepper callback semantics. Note that
// the "owning" |CallbackTracker| will keep a reference until the callback is
// completed.
//
// Subclasses must do several things:
//  - They must ensure that the callback is executed at most once (by looking at
//    |completed()| before running the callback).
//  - They must ensure that the callback is run abortively if it is marked as to
//    be aborted (by looking at |aborted()| before running the callback).
//  - They must call |MarkAsCompleted()| immediately before actually running the
//    callback; see the comment for |MarkAsCompleted()| for a caveat.
class PPAPI_SHARED_EXPORT TrackedCallback
    : public base::RefCountedThreadSafe<TrackedCallback> {
 public:
  // Create a tracked completion callback and register it with the tracker. The
  // resource pointer is not stored. If |resource| is NULL, this callback will
  // not be added to the callback tracker.
  TrackedCallback(Resource* resource, const PP_CompletionCallback& callback);

  // These run the callback in an abortive manner, or post a task to do so (but
  // immediately marking the callback as to be aborted).
  void Abort();
  void PostAbort();

  // Run the callback with the given result. If the callback had previously been
  // marked as to be aborted (by |PostAbort()|), |result| will be ignored and
  // the callback will be run with result |PP_ERROR_ABORTED|.
  //
  // See also ClearAndRun().
  void Run(int32_t result);
  void PostRun(int32_t result);

  void BlockUntilRun();

  // Returns the ID of the resource which "owns" the callback, or 0 if the
  // callback is not associated with any resource.
  PP_Resource resource_id() const { return resource_id_; }

  // Returns true if the callback was completed (possibly aborted).
  bool completed() const { return completed_; }

  // Returns true if the callback was or should be aborted; this will be the
  // case whenever |Abort()| or |PostAbort()| is called before a non-abortive
  // completion.
  bool aborted() const { return aborted_; }

  // Helper to determine if the given callback is set and not yet completed.
  // The normal pattern is to use a scoped_refptr to hold a callback. This
  // function tells you if the operation is currently in progress by checking
  // both the null-ness of the scoped_refptr, as well as the completion state
  // of the callback (which may still be out-standing via a PostAbort).
  static bool IsPending(const scoped_refptr<TrackedCallback>& callback);

  // Runs the given callback, clearing the given scoped_refptr before execution.
  // This is useful for cases where there can be only one pending callback, and
  // the presence of the callback indicates one is pending. Such code would
  // normally want to clear it before execution so the plugin can issue a new
  // request.
  static void ClearAndRun(scoped_refptr<TrackedCallback>* callback,
                          int32_t result);

  // Same as ClearAndRun except it calls Abort().
  static void ClearAndAbort(scoped_refptr<TrackedCallback>* callback);

 protected:
  bool is_blocking() {
    return !callback_.func;
  }
  bool is_required() {
    return (callback_.func &&
            !(callback_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL));
  }
  bool is_optional() {
    return (callback_.func &&
            (callback_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL));
  }

 private:
  // TrackedCallback and EnterBase manage dealing with how to invoke callbacks
  // appropriately. Pepper interface implementations and proxies should not have
  // to check the type of callback, block, or mark them complete explicitly.
  friend class ppapi::thunk::subtle::EnterBase;

  // Block until the associated operation has completed. Returns the result.
  // This must only be called on a non-main thread on a blocking callback.
  int32_t BlockUntilComplete();

  // Mark this object as complete and remove it from the tracker. This must only
  // be called once. Note that running this may result in this object being
  // deleted (so keep a reference if it'll still be needed).
  void MarkAsCompleted();

  // This class is ref counted.
  friend class base::RefCountedThreadSafe<TrackedCallback>;
  virtual ~TrackedCallback();

  // Factory used by |PostAbort()| and |PostRun()|. Note that it's safe to
  // cancel any pending posted tasks on destruction -- before it's destroyed,
  // the "owning" |CallbackTracker| must have gone through and done
  // (synchronous) |Abort()|s.
  base::WeakPtrFactory<TrackedCallback> weak_ptr_factory_;

  scoped_refptr<CallbackTracker> tracker_;
  PP_Resource resource_id_;
  bool completed_;
  bool aborted_;
  PP_CompletionCallback callback_;

  int32_t result_for_blocked_callback_;
  // Used for pausing/waking the blocked thread if this is a blocking completion
  // callback. Note that in-process, there is no lock, blocking callbacks are
  // not allowed, and therefore this pointer will be NULL.
  scoped_ptr<base::ConditionVariable> operation_completed_condvar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TrackedCallback);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_TRACKED_CALLBACK_H_
