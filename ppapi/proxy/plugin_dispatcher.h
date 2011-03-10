// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <string>

#include "base/hash_tables.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"

class MessageLoop;

namespace base {
class WaitableEvent;
}

namespace pp {
namespace proxy {

// Used to keep track of per-instance data.
struct InstanceData {
  PP_Rect position;
};

class PluginDispatcher : public Dispatcher {
 public:
  // Constructor for the plugin side. The init and shutdown functions will be
  // will be automatically called when requested by the renderer side. The
  // module ID will be set upon receipt of the InitializeModule message.
  //
  // You must call Dispatcher::InitWithChannel after the constructor.
  PluginDispatcher(base::ProcessHandle remote_process_handle,
                   GetInterfaceFunc get_interface);
  virtual ~PluginDispatcher();

  // The plugin side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel if there are multiple
  // renderers sharing the same plugin. This mapping is maintained by
  // DidCreateInstance/DidDestroyInstance.
  static PluginDispatcher* GetForInstance(PP_Instance instance);

  static const void* GetInterfaceFromDispatcher(const char* interface);

  // Dispatcher overrides.
  virtual bool InitWithChannel(Delegate* delegate,
                               const IPC::ChannelHandle& channel_handle,
                               bool is_client);
  virtual bool IsPlugin() const;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Keeps track of which dispatcher to use for each instance, active instances
  // and tracks associated data like the current size.
  void DidCreateInstance(PP_Instance instance);
  void DidDestroyInstance(PP_Instance instance);

  // Gets the data for an existing instance, or NULL if the instance id doesn't
  // correspond to  a known instance.
  InstanceData* GetInstanceData(PP_Instance instance);

#if defined(OS_POSIX)
  // See renderer_fd_ below.
  int GetRendererFD();
  void CloseRendererFD();
#endif

 private:
  friend class PluginDispatcherTest;

  // Notifies all live instances that they're now closed. This is used when
  // a renderer crashes or some other error is received.
  void ForceFreeAllInstances();

  // IPC message handlers.
  void OnMsgSupportsInterface(const std::string& interface_name, bool* result);

#if defined(OS_POSIX)
  // FD for the renderer end of the socket. It is closed when the IPC layer
  // indicates that the channel is connected, proving that the renderer has
  // access to its side of the socket.
  int renderer_fd_;
#endif

  // All target proxies currently created. These are ones that receive
  // messages.
  scoped_ptr<InterfaceProxy> target_proxies_[INTERFACE_ID_COUNT];

  typedef base::hash_map<PP_Instance, InstanceData> InstanceDataMap;
  InstanceDataMap instance_map_;

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
