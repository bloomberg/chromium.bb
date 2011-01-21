// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <string>

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"

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
  PluginDispatcher(base::ProcessHandle remote_process_handle,
                   GetInterfaceFunc get_interface,
                   InitModuleFunc init_module,
                   ShutdownModuleFunc shutdown_module);
  ~PluginDispatcher();

  // Sets/gets the global dispatcher pointer. New code should use the
  // GetForInstance version below, this is currently here as a stopgap while
  // the transition is being made.
  //
  // TODO(brettw) remove this.
  static PluginDispatcher* Get();
  static void SetGlobal(PluginDispatcher* dispatcher);

  // The plugin side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel if there are multiple
  // renderers sharing the same plugin.
  static PluginDispatcher* GetForInstance(PP_Instance instance);
  /* TODO(brettw) enable this when Get() is removed.
  static void SetForInstance(PP_Instance instance,
                             PluginDispatcher* dispatcher);
  static void RemoveForInstance(PP_Instance instance);
  */

  // Dispatcher overrides.
  virtual bool IsPlugin() const;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // IPC message handlers.
  void OnMsgInitializeModule(PP_Module pp_module, bool* result);
  void OnMsgShutdown();

  InitModuleFunc init_module_;
  ShutdownModuleFunc shutdown_module_;

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
