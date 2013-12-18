// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_

#include "remoting/base/plugin_thread_task_runner.h"

namespace pp {
class Core;
}  // namespace pp

namespace remoting {

class PepperPluginThreadDelegate : public PluginThreadTaskRunner::Delegate {
 public:
  PepperPluginThreadDelegate();
  virtual ~PepperPluginThreadDelegate();

  virtual bool RunOnPluginThread(
      base::TimeDelta delay, void(CDECL function)(void*), void* data) OVERRIDE;

 private:
  pp::Core* core_;
};

} // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_
