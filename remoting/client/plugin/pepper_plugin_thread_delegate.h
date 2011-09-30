// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_

#include "remoting/base/plugin_message_loop_proxy.h"

// Macro useful for writing cross-platform function pointers.
#if defined(OS_WIN) && !defined(CDECL)
#define CDECL __cdecl
#else
#define CDECL
#endif

namespace pp {
class Core;
}  // namespace pp

namespace remoting {

class PepperPluginThreadDelegate : public PluginMessageLoopProxy::Delegate {
 public:
  PepperPluginThreadDelegate();
  virtual ~PepperPluginThreadDelegate();

  virtual bool RunOnPluginThread(
      int delay_ms, void(CDECL function)(void*), void* data) OVERRIDE;

 private:
  pp::Core* core_;
};

} // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_PLUGIN_THREAD_DELEGATE_H_
