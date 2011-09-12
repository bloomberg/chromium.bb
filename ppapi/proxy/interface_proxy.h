// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_PROXY_H_
#define PPAPI_PROXY_INTERFACE_PROXY_H_

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/shared_impl/function_group_base.h"

namespace ppapi {
namespace proxy {

class Dispatcher;

class InterfaceProxy : public IPC::Channel::Listener,
                       public IPC::Message::Sender,
                       public FunctionGroupBase {
 public:
  // Factory function type for interfaces. Ownership of the returned pointer
  // is transferred to the caller.
  typedef InterfaceProxy* (*Factory)(Dispatcher* dispatcher);

  // DEPRECATED: New classes should be registered directly in the interface
  // list. This is kept around until we convert all the existing code.
  //
  // Information about the interface. Each interface has a static function to
  // return its info, which allows either construction on the target side, and
  // getting the proxied interface on the source side (see dispatcher.h for
  // terminology).
  struct Info {
    const void* interface_ptr;

    const char* name;
    InterfaceID id;

    bool is_trusted;

    InterfaceProxy::Factory create_proxy;
  };

  virtual ~InterfaceProxy();

  Dispatcher* dispatcher() const { return dispatcher_; }

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // Sub-classes must implement IPC::Channel::Listener which contains this:
  //virtual bool OnMessageReceived(const IPC::Message& msg);

 protected:
  // Creates the given interface associated with the given dispatcher. The
  // dispatcher manages our lifetime.
  InterfaceProxy(Dispatcher* dispatcher);

  uint32 SendCallback(PP_CompletionCallback callback);
  PP_CompletionCallback ReceiveCallback(uint32 serialized_callback);

 private:
  Dispatcher* dispatcher_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_PROXY_H_

