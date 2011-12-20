// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
#define REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

#include "ppapi/cpp/completion_callback.h"

namespace remoting {

// Function for adapting a Chromium base::Closure to a PP_CompletionCallback
// friendly function.  The base::Closure object should be a dynamically
// allocated copy of the result from base::Bind(). It should be passed as
// |user_data|.  This function will invoke base::Closure::Run() on
// |user_data| when called, and then delete |user_data|.
void CompletionCallbackClosureAdapter(void* user_data, int32_t not_used);

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
