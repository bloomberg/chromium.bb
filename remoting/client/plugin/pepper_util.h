// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
#define REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

#include "ppapi/cpp/completion_callback.h"

namespace remoting {

// Adapts a base::Callback to a pp::CompletionCallback, which may be passed to
// exactly one Pepper API. If the adapted callback is not used then a copy of
// |callback| will be leaked, including references & passed values bound to it.
pp::CompletionCallback PpCompletionCallback(base::Callback<void(int)> callback);

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
