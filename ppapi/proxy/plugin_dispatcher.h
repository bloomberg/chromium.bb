// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "ppapi/proxy/callback_tracker.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"

class MessageLoop;

namespace base {
class WaitableEvent;
}

namespace pp {
namespace proxy {

class PluginDispatcher : public Dispatcher {
 public:
  // Constructor for the plugin side. The init and shutdown functions will be
  // will be automatically called when requested by the renderer side. The
  // module ID will be set upon receipt of the InitializeModule message.
  //
  // You must call Dispatcher::InitWithChannel after the constructor.
  PluginDispatcher(GetInterfaceFunc get_interface,
                   InitModuleFunc init_module,
                   ShutdownModuleFunc shutdown_module);
  ~PluginDispatcher();

  // The plugin maintains a global Dispatcher pointer. There is only one since
  // there is only one connection to the browser. Don't call this on the
  // browser side, see GetForInstnace.
  static PluginDispatcher* Get();
  static void SetGlobal(PluginDispatcher* dispatcher);

  // Dispatcher overrides.
  virtual bool IsPlugin() const { return true; }

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Returns the resource tracker for the plugin. In the browser process this
  // will return NULL.
  PluginResourceTracker* plugin_resource_tracker() {
    return plugin_resource_tracker_.get();
  }

  // Returns the var tracker for the plugin. In the browser process this
  // will return NULL.
  PluginVarTracker* plugin_var_tracker() {
    return plugin_var_tracker_.get();
  }

 private:
  void OnInitializeModule(PP_Module pp_module, bool* result);

  InitModuleFunc init_module_;
  ShutdownModuleFunc shutdown_module_;

  scoped_ptr<PluginResourceTracker> plugin_resource_tracker_;
  scoped_ptr<PluginVarTracker> plugin_var_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
