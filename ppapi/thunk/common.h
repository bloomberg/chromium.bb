// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_COMMON_H_
#define PPAPI_THUNK_COMMON_H_

#include "ppapi/c/pp_completion_callback.h"

namespace ppapi {
namespace thunk {

// Skips callback invocation and returns |result| if callback function is NULL
// or PP_COMPLETIONCALLBACK_FLAG_OPTIONAL is set. Otherwise, schedules the
// callback with |result| as an argument and returns PP_OK_COMPLETIONPENDING.
int32_t MayForceCallback(PP_CompletionCallback callback, int32_t result);

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_COMMON_H_
