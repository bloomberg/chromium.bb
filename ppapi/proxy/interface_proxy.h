// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

namespace pp {
namespace proxy {

class Dispatcher;

class InterfaceProxy : public IPC::Channel::Listener,
                       public IPC::Message::Sender {
 public:
  // Factory function type for interfaces. Ownership of the returned pointer
  // is transferred to the caller.
  typedef InterfaceProxy* (*Factory)(Dispatcher* dispatcher,
                                     const void* target_interface);

  // Information about the interface. Each interface has a static function to
  // return its info, which allows either construction on the target side, and
  // getting the proxied interface on the source side (see dispatcher.h for
  // terminology).
  struct Info {
    const void* interface;

    const char* name;
    InterfaceID id;

    bool is_trusted;

    InterfaceProxy::Factory create_proxy;
  };

  virtual ~InterfaceProxy();

  // The actual implementation of the given interface in the current process.
  const void* target_interface() const { return target_interface_; }

  Dispatcher* dispatcher() const { return dispatcher_; }

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // Sub-classes must implement IPC::Channel::Listener which contains this:
  //virtual bool OnMessageReceived(const IPC::Message& msg);

 protected:
  // Creates the given interface associated with the given dispatcher. The
  // dispatcher manages our lifetime.
  //
  // The target interface pointer, when non-NULL, indicates that this is a
  // target proxy (see dispatcher.h for a definition).  In this case, the proxy
  // will interpret this pointer to the actual implementation of the interface
  // in the local process.
  InterfaceProxy(Dispatcher* dispatcher, const void* target_interface);

  uint32 SendCallback(PP_CompletionCallback callback);
  PP_CompletionCallback ReceiveCallback(uint32 serialized_callback);

 private:
  Dispatcher* dispatcher_;
  const void* target_interface_;
};

inline PP_Bool BoolToPPBool(bool value) {
  return value ? PP_TRUE : PP_FALSE;
}

inline bool PPBoolToBool(PP_Bool value) {
  return (PP_TRUE == value);
}

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_INTERFACE_PROXY_H_

