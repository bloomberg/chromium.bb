// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_util.h"

#include "base/callback.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/cpp/module.h"

namespace remoting {

static void CallbackAdapter(void* user_data, int32_t result) {
  scoped_ptr<base::Callback<void(int)> > callback(
      reinterpret_cast<base::Callback<void(int)>*>(user_data));
  callback->Run(result);
}

pp::CompletionCallback PpCompletionCallback(
    base::Callback<void(int)> callback) {
  return pp::CompletionCallback(&CallbackAdapter,
                                new base::Callback<void(int)>(callback));
}

}  // namespace remoting
