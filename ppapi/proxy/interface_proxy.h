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
  // Creates the given interface associated with the given dispatcher. The
  // dispatcher manages our lifetime.
  //
  // The target interface pointer, when non-NULL, indicates that this is a
  // target proxy (see dispatcher.h for a definition).  In this case, the proxy
  // will interpret this pointer to the actual implementation of the interface
  // in the local process.
  //
  // If the target interface is NULL, this proxy will be a "source" interface.
  InterfaceProxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~InterfaceProxy();

  // See dispatcher.h for definitions of source and target.
  bool is_source_proxy() const { return !target_interface_; }
  bool is_target_proxy() const { return !!target_interface_; }

  // When this proxy is the "target" of the IPC communication (see
  // dispatcher.h), this target_interface pointer will indicate the local
  // side's interface pointer. This contains the functions that actually
  // implement the proxied interface.
  //
  // This will be NULL when this proxy is a source proxy.
  const void* target_interface() const { return target_interface_; }

  Dispatcher* dispatcher() const { return dispatcher_; }

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // Returns the local implementation of the interface that will proxy it to
  // the remote side. This is used on the source side only (see dispatcher.h).
  virtual const void* GetSourceInterface() const = 0;

  // Returns the interface ID associated with this proxy. Implemented by each
  // derived class to identify itself.
  virtual InterfaceID GetInterfaceId() const = 0;

  // Sub-classes must implement IPC::Channel::Listener which contains this:
  //virtual bool OnMessageReceived(const IPC::Message& msg);

 protected:
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

