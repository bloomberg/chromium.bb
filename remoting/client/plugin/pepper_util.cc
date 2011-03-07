// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_util.h"

#include "base/task.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/cpp/module.h"

namespace remoting {

void CompletionCallbackTaskAdapter(void* user_data, int32_t not_used) {
  Task* task = reinterpret_cast<Task*>(user_data);
  task->Run();
  delete task;
}

pp::CompletionCallback TaskToCompletionCallback(Task* task) {
  return pp::CompletionCallback(&CompletionCallbackTaskAdapter, task);
}

void RunTaskOnPluginThread(Task* task) {
  pp::Module::Get()->core()->CallOnMainThread(
      0 /* run immediately */,
      TaskToCompletionCallback(task),
      0 /* unused value */
      );
}

bool CurrentlyOnPluginThread() {
  return pp::Module::Get()->core()->IsMainThread();
}

}  // namespace remoting
