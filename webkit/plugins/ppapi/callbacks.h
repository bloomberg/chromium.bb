// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_CALLBACKS_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_CALLBACKS_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace webkit {
namespace ppapi {

class TrackedCallback;

// Pepper callbacks have the following semantics (unless otherwise specified;
// in particular, the below apply to all completion callbacks):
//  - Callbacks are always run on the main thread.
//  - Callbacks are always called from the main message loop. In particular,
//    calling into Pepper will not result in the plugin being re-entered via a
//    synchronously-run callback.
//  - Each callback will be executed (a.k.a. completed) exactly once.
//  - Each callback may be *aborted*, which means that it will be executed with
//    result |PP_ERROR_ABORTED| (in the case of completion callbacks).
//  - Before |PPP_ShutdownModule()| is called, every pending callback (for that
//    module) will be aborted.
//  - Callbacks are usually associated to a resource, whose "deletion" provides
//    a "cancellation" (or abort) mechanism -- see below.
//  - When a plugin releases its last reference to resource, all callbacks
//    associated to that resource are aborted. Even if a non-abortive completion
//    of such a callback had previously been scheduled (i.e., posted), that
//    callback must now be aborted. The aborts should be scheduled immediately
//    (upon the last reference being given up) and should not rely on anything
//    else (e.g., a background task to complete or further action from the
//    plugin).
//  - Abortive completion gives no information about the status of the
//    asynchronous operation: The operation may have not yet begun, may be in
//    progress, or may be completed (successfully or not). In fact, the
//    operation may still proceed after the callback has been aborted.
//  - Any output data buffers provided to Pepper are associated with a resource.
//    Once that resource is released, no subsequent writes to those buffers. (If
//    background threads are set up to write into such buffers, the final
//    release operation should not return into the plugin until it can
//    guaranteed that those threads will no longer write into the buffers.)
//
// Thread-safety notes:
// Currently, everything should happen on the main thread. The objects are
// thread-safe ref-counted, so objects which live on different threads may keep
// references. Releasing a reference to |TrackedCallback| on a different thread
// (possibly causing destruction) is also okay. Otherwise, all methods should be
// called only from the main thread.

// |CallbackTracker| tracks pending Pepper callbacks for a single module. It
// also tracks, for each resource ID, which callbacks are pending. When a
// callback is (just about to be) completed, it is removed from the tracker. We
// use |CallbackTracker| for two things: (1) to ensure that all callbacks are
// properly aborted before module shutdown, and (2) to ensure that all callbacks
// associated to a given resource are aborted when a plugin (module) releases
// its last reference to that resource.
class CallbackTracker : public base::RefCountedThreadSafe<CallbackTracker> {
 public:
  CallbackTracker();

  // Abort all callbacks (synchronously).
  void AbortAll();

  // Abort all callbacks associated to the given resource ID (which must be
  // valid, i.e., nonzero) by posting a task (or tasks).
  void PostAbortForResource(PP_Resource resource_id);

 private:
  friend class base::RefCountedThreadSafe<CallbackTracker>;
  WEBKIT_PLUGINS_EXPORT ~CallbackTracker();

  // |TrackedCallback| are expected to automatically add and
  // remove themselves from their provided |CallbackTracker|.
  friend class TrackedCallback;
  void Add(const scoped_refptr<TrackedCallback>& tracked_callback);
  void Remove(const scoped_refptr<TrackedCallback>& tracked_callback);

  // For each resource ID with a pending callback, store a set with its pending
  // callbacks. (Resource ID 0 is used for callbacks not associated to a valid
  // resource.) If a resource ID is re-used for another resource, there may be
  // aborted callbacks corresponding to the original resource in that set; these
  // will be removed when they are completed (abortively).
  typedef std::set<scoped_refptr<TrackedCallback> > CallbackSet;
  typedef std::map<PP_Resource, CallbackSet> CallbackSetMap;
  CallbackSetMap pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CallbackTracker);
};

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
class TrackedCallback : public base::RefCountedThreadSafe<TrackedCallback> {
 public:
  // The constructor will add the new object to the tracker. The resource ID is
  // optional -- set it to 0 if no resource is associated to the callback.
  TrackedCallback(const scoped_refptr<CallbackTracker>& tracker,
                  PP_Resource resource_id);

  // These run the callback in an abortive manner, or post a task to do so (but
  // immediately marking the callback as to be aborted).
  WEBKIT_PLUGINS_EXPORT void Abort();
  void PostAbort();

  // Returns the ID of the resource which "owns" the callback, or 0 if the
  // callback is not associated with any resource.
  PP_Resource resource_id() const { return resource_id_; }

  // Returns true if the callback was completed (possibly aborted).
  bool completed() const { return completed_; }

  // Returns true if the callback was or should be aborted; this will be the
  // case whenever |Abort()| or |PostAbort()| is called before a non-abortive
  // completion.
  bool aborted() const { return aborted_; }

 protected:
  // This class is ref counted.
  friend class base::RefCountedThreadSafe<TrackedCallback>;
  virtual ~TrackedCallback();

  // To be implemented by subclasses: Actually run the callback abortively.
  virtual void AbortImpl() = 0;

  // Mark this object as complete and remove it from the tracker. This must only
  // be called once. Note that running this may result in this object being
  // deleted (so keep a reference if it'll still be needed).
  void MarkAsCompleted();

  // Factory used by |PostAbort()|. Note that it's safe to cancel any pending
  // posted aborts on destruction -- before it's destroyed, the "owning"
  // |CallbackTracker| must have gone through and done (synchronous) |Abort()|s.
  base::WeakPtrFactory<TrackedCallback> abort_impl_factory_;

 private:
  scoped_refptr<CallbackTracker> tracker_;
  PP_Resource resource_id_;
  bool completed_;
  bool aborted_;

  DISALLOW_COPY_AND_ASSIGN(TrackedCallback);
};

// |TrackedCompletionCallback| represents a tracked Pepper completion callback.
class TrackedCompletionCallback : public TrackedCallback {
 public:
  // Create a tracked completion callback and register it with the tracker. The
  // resource ID may be 0 if the callback is not associated to any resource.
  WEBKIT_PLUGINS_EXPORT TrackedCompletionCallback(
      const scoped_refptr<CallbackTracker>& tracker,
      PP_Resource resource_id,
      const PP_CompletionCallback& callback);

  // Run the callback with the given result. If the callback had previously been
  // marked as to be aborted (by |PostAbort()|), |result| will be ignored and
  // the callback will be run with result |PP_ERROR_ABORTED|.
  WEBKIT_PLUGINS_EXPORT void Run(int32_t result);

 protected:
  // |TrackedCallback| method:
  virtual void AbortImpl();

 private:
  PP_CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TrackedCompletionCallback);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_CALLBACKS_H_
