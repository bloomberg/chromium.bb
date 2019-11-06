// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_

#include <map>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_tracker_internal.h"

namespace sync_file_system {
namespace drive_backend {

// A helper class to ensure one-shot callback to be called exactly once.
//
// Usage:
//   class Foo {
//    private:
//     CallbackTracker callback_tracker_;
//   };
//
//   void DoSomethingAsync(const SomeCallbackType& callback) {
//     base::Closure abort_case_handler = base::Bind(callback, ABORT_ERROR);
//
//     SomeCallbackType wrapped_callback =
//         callback_tracker_.Register(
//             abort_case_handler, callback);
//
//     // The body of the operation goes here.
//   }
class CallbackTracker {
 public:
  typedef std::map<internal::AbortHelper*, base::Closure> AbortClosureByHelper;

  CallbackTracker();
  ~CallbackTracker();

  // Returns a wrapped callback.
  // Upon AbortAll() call, CallbackTracker invokes |abort_closure| and voids all
  // wrapped callbacks returned by Register().
  // Invocation of the wrapped callback unregisters |callback| from
  // CallbackTracker.
  template <typename T>
  base::Callback<T> Register(const base::Closure& abort_closure,
                             const base::Callback<T>& callback) {
    internal::AbortHelper* helper = new internal::AbortHelper(this);
    helpers_[helper] = abort_closure;
    return base::Bind(&internal::InvokeAndInvalidateHelper<T>::Run,
                      helper->AsWeakPtr(), callback);
  }

  void AbortAll();

 private:
  friend class internal::AbortHelper;

  std::unique_ptr<internal::AbortHelper> PassAbortHelper(
      internal::AbortHelper* helper);

  AbortClosureByHelper helpers_;  // Owns AbortHelpers.

  DISALLOW_COPY_AND_ASSIGN(CallbackTracker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_
