// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/common.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace thunk {

int32_t MayForceCallback(PP_CompletionCallback callback, int32_t result) {
  if (result == PP_OK_COMPLETIONPENDING)
    return result;

  if (callback.func == NULL ||
      (callback.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) != 0)
    return result;

  // TODO(polina): make this work off the main thread as well
  // (At this point this should not be an issue because PPAPI is only supported
  // on the main thread).
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      callback.func, callback.user_data, result));
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace thunk
}  // namespace ppapi
