/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DELAYED_CALLBACK_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DELAYED_CALLBACK_H_

#include "native_client/src/include/portability.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

// Delay a "Run(PP_OK)" of a pp::CompletionCallback after exactly N time ticks
// have passed. User should not attempt to count beyond N ticks.
// It is expected that this is only counted up on "good" code paths
// (when there are no errors), so that passing along a status code of
// PP_OK makes sense.
class DelayedCallback {
 public:
  DelayedCallback(pp::CompletionCallback continuation,
                  uint32_t initial_requirement)
    : required_ticks_(initial_requirement),
      continuation_(continuation),
      started_(false) {
  }

  ~DelayedCallback() { }

  // Advance time, and run the callback if it is finally time.
  // This must be run on the main thread, since pp callbacks
  // must be run on the main thread anyway. We also want to
  // avoid race condtions. If we want to relax the requirement,
  // we could add locks and use CallOnMainThread.
  void RunIfTime() {
    CHECK(required_ticks_ > 0);
    started_ = true;
    --required_ticks_;
    if (0 == required_ticks_) {
      continuation_.Run(PP_OK);
    }
  }

  // Add another requirement before anything has run.
  void IncrRequirements(uint32_t additional_requirements) {
    CHECK(started_ == false);
    required_ticks_ += additional_requirements;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(DelayedCallback);

  // How many time ticks are left before we run the callback.
  uint32_t required_ticks_;

  // The continuation to invoke when ticks passed.
  pp::CompletionCallback continuation_;

  // Some sanity checking to catch when people misuse the library.
  bool started_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DELAYED_CALLBACK_H_
