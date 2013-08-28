// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_net_address_api.h"

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterResourceNoLock<thunk::PPB_NetAddress_API>
    EnterNetAddressNoLock;

}  // namespace

TCPSocketResource::TCPSocketResource(Connection connection,
                                     PP_Instance instance)
    : TCPSocketResourceBase(connection, instance, false) {
  SendCreate(BROWSER, PpapiHostMsg_TCPSocket_Create());
}

TCPSocketResource::~TCPSocketResource() {
  DisconnectImpl();
}

thunk::PPB_TCPSocket_API* TCPSocketResource::AsPPB_TCPSocket_API() {
  return this;
}

int32_t TCPSocketResource::Connect(PP_Resource addr,
                                   scoped_refptr<TrackedCallback> callback) {
  EnterNetAddressNoLock enter(addr, true);
  if (enter.failed())
    return PP_ERROR_BADARGUMENT;

  return ConnectWithNetAddressImpl(&enter.object()->GetNetAddressPrivate(),
                                   callback);
}

PP_Resource TCPSocketResource::GetLocalAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetLocalAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

PP_Resource TCPSocketResource::GetRemoteAddress() {
  PP_NetAddress_Private addr_private;
  if (!GetRemoteAddressImpl(&addr_private))
    return 0;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetAddressFromNetAddressPrivate(
      pp_instance(), addr_private);
}

int32_t TCPSocketResource::Read(char* buffer,
                                int32_t bytes_to_read,
                                scoped_refptr<TrackedCallback> callback) {
  return ReadImpl(buffer, bytes_to_read, callback);
}

int32_t TCPSocketResource::Write(const char* buffer,
                                 int32_t bytes_to_write,
                                 scoped_refptr<TrackedCallback> callback) {
  return WriteImpl(buffer, bytes_to_write, callback);
}

void TCPSocketResource::Close() {
  DisconnectImpl();
}

int32_t TCPSocketResource::SetOption(PP_TCPSocket_Option name,
                                     const PP_Var& value,
                                     scoped_refptr<TrackedCallback> callback) {
  return SetOptionImpl(name, value, callback);
}

}  // namespace proxy
}  // namespace ppapi
