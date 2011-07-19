// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
#define REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_

#include "base/basictypes.h"

#include "ppapi/cpp/completion_callback.h"

class Task;

namespace remoting {

// Function for adapting a Chromium style Task into a
// PP_CompletionCallback friendly function.  The Task object should be passed
// as |user_data|.  This function will invoke Task::Run() on |user_data| when
// called, and then delete |user_data|.
void CompletionCallbackTaskAdapter(void* user_data, int32_t not_used);

// Converts a Task* to a pp::CompletionCallback suitable for use with ppapi C++
// APIs that require a pp::CompletionCallback.  Takes ownership of |task|.
pp::CompletionCallback TaskToCompletionCallback(Task* task);

// Posts the current task to the plugin's main thread.  Takes ownership of
// |task|.
void RunTaskOnPluginThread(Task* task);

// Returns true if the current thread is the plugin main thread.
bool CurrentlyOnPluginThread();

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PLUGIN_UTIL_H_
