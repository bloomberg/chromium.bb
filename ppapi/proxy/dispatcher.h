// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DISPATCHER_H_
#define PPAPI_PROXY_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/linked_ptr.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/callback_tracker.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_var_tracker.h"

class MessageLoop;
struct PPB_Var_Deprecated;

namespace base {
class WaitableEvent;
}

namespace IPC {
class SyncChannel;
class TestSink;
}

namespace pp {
namespace proxy {

class VarSerializationRules;

// An interface proxy can represent either end of a cross-process interface
// call. The "source" side is where the call is invoked, and the "target" side
// is where the call ends up being executed.
//
// Plugin side                          | Browser side
// -------------------------------------|--------------------------------------
//                                      |
//    "Source"                          |    "Target"
//    InterfaceProxy ----------------------> InterfaceProxy
//                                      |
//                                      |
//    "Target"                          |    "Source"
//    InterfaceProxy <---------------------- InterfaceProxy
//                                      |
class Dispatcher : public IPC::Channel::Listener,
                   public IPC::Message::Sender {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int32_t (*InitModuleFunc)(PP_Module, GetInterfaceFunc);
  typedef void (*ShutdownModuleFunc)();

  virtual ~Dispatcher();

  // You must call this function before anything else. Returns true on success.
  bool InitWithChannel(MessageLoop* ipc_message_loop,
                       const IPC::ChannelHandle& channel_handle,
                       bool is_client,
                       base::WaitableEvent* shutdown_event);

  // Alternative to InitWithChannel() for unit tests that want to send all
  // messages sent via this dispatcher to the given test sink. The test sink
  // must outlive this class.
  void InitWithTestSink(IPC::TestSink* test_sink);

  // Returns true if the dispatcher is on the plugin side, or false if it's the
  // browser side.
  virtual bool IsPlugin() const = 0;

  VarSerializationRules* serialization_rules() const {
    return serialization_rules_.get();
  }

  // Wrapper for calling the local GetInterface function.
  const void* GetLocalInterface(const char* interface);

  // Returns the remote process' handle. For the host dispatcher, this will be
  // the plugin process, and for the plugin dispatcher, this will be the
  // renderer process. This is used for sharing memory and such and is
  // guaranteed valid (unless the remote process has suddenly died).
  base::ProcessHandle remote_process_handle() const {
    return remote_process_handle_;
  }

  // Called if the remote side is declaring to us which interfaces it supports
  // so we don't have to query for each one. We'll pre-create proxies for
  // each of the given interfaces.

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Will be NULL in some unit tests and if the remote side has crashed.
  IPC::SyncChannel* channel() const {
    return channel_.get();
  }

  CallbackTracker& callback_tracker() {
    return callback_tracker_;
  }

  // Retrieves the information associated with the given interface, identified
  // either by name or ID. Each function searches either PPP or PPB interfaces.
  static const InterfaceProxy::Info* GetPPBInterfaceInfo(
      const std::string& name);
  static const InterfaceProxy::Info* GetPPBInterfaceInfo(
      InterfaceID id);
  static const InterfaceProxy::Info* GetPPPInterfaceInfo(
      const std::string& name);
  static const InterfaceProxy::Info* GetPPPInterfaceInfo(
      InterfaceID id);

 protected:
  Dispatcher(base::ProcessHandle remote_process_handle,
             GetInterfaceFunc local_get_interface);

  // Setter for the derived classes to set the appropriate var serialization.
  // Takes ownership of the given pointer, which must be on the heap.
  void SetSerializationRules(VarSerializationRules* var_serialization_rules);

  bool disallow_trusted_interfaces() const {
    return disallow_trusted_interfaces_;
  }

 private:
  base::ProcessHandle remote_process_handle_;  // See getter above.

  // When we're unit testing, this will indicate the sink for the messages to
  // be deposited so they can be inspected by the test. When non-NULL, this
  // indicates that the channel should not be used.
  IPC::TestSink* test_sink_;

  // Will be null for some tests when there is a test_sink_, and if the
  // remote side has crashed.
  scoped_ptr<IPC::SyncChannel> channel_;

  bool disallow_trusted_interfaces_;

  GetInterfaceFunc local_get_interface_;

  CallbackTracker callback_tracker_;

  scoped_ptr<VarSerializationRules> serialization_rules_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_DISPATCHER_H_
