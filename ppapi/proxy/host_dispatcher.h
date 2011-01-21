// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_DISPATCHER_H_
#define PPAPI_PROXY_HOST_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/plugin_var_tracker.h"

struct PPB_Var_Deprecated;

namespace base {
class WaitableEvent;
}

namespace IPC {
class SyncChannel;
}

namespace pp {
namespace proxy {

class InterfaceProxy;
class VarSerialization;

class HostDispatcher : public Dispatcher {
 public:
  // Constructor for the renderer side.
  //
  // You must call Dispatcher::InitWithChannel after the constructor.
  HostDispatcher(base::ProcessHandle host_process_handle,
                 PP_Module module,
                 GetInterfaceFunc local_get_interface);
  ~HostDispatcher();

  // Calls the plugin's PPP_InitializeModule function and returns true if
  // the call succeeded.
  bool InitializeModule();

  // The host side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel.
  static HostDispatcher* GetForInstance(PP_Instance instance);
  static void SetForInstance(PP_Instance instance,
                             HostDispatcher* dispatcher);
  static void RemoveForInstance(PP_Instance instance);

  // Dispatcher overrides.
  virtual bool IsPlugin() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_HOST_DISPATCHER_H_
