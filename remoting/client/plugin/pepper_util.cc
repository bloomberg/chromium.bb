// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_util.h"

#include "base/callback.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/cpp/module.h"

namespace remoting {

void CompletionCallbackClosureAdapter(void* user_data, int32_t not_used) {
  base::Closure* closure = reinterpret_cast<base::Closure*>(user_data);
  closure->Run();
  delete closure;
}

}  // namespace remoting
