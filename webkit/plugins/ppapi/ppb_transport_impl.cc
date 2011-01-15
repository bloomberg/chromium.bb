// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_transport_impl.h"

#include "base/singleton.h"
#include "base/threading/thread_local.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

// Creates a new transport object with the specified name
// using the specified protocol.
PP_Resource CreateTransport(PP_Instance instance,
                            const char* name,
                            const char* proto) {
  // TODO(juberti): implement me
  PP_Resource p(0);
  return p;
}

// Returns whether or not resource is PPB_Transport_Impl
PP_Bool IsTransport(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Transport_Impl>(resource));
}

// Returns whether the transport is currently writable
// (i.e. can send data to the remote peer)
PP_Bool IsWritable(PP_Resource transport) {
  // TODO(juberti): impelement me
  return PP_FALSE;
}


// TODO(juberti): other getters/setters
// connect state
// connect type, protocol
// RTT


// Establishes a connection to the remote peer.
// Returns PP_ERROR_WOULDBLOCK and notifies on |cb|
// when connectivity is established (or timeout occurs).
int32_t Connect(PP_Resource transport,
                PP_CompletionCallback cb) {
  // TODO(juberti): impelement me
  return 0;
}


// Obtains another ICE candidate address to be provided
// to the remote peer. Returns PP_ERROR_WOULDBLOCK
// if there are no more addresses to be sent.
int32_t GetNextAddress(PP_Resource transport,
                       PP_Var* address,
                       PP_CompletionCallback cb) {
  // TODO(juberti): implement me
  return 0;
}


// Provides an ICE candidate address that was received
// from the remote peer.
int32_t ReceiveRemoteAddress(PP_Resource transport,
                             PP_Var address) {
  // TODO(juberti): implement me
  return 0;
}


// Like recv(), receives data. Returns PP_ERROR_WOULDBLOCK
// if there is currently no data to receive.
int32_t Recv(PP_Resource transport,
             void* data,
             uint32_t len,
             PP_CompletionCallback cb) {
  // TODO(juberti): implement me
  return 0;
}


// Like send(), sends data. Returns PP_ERROR_WOULDBLOCK
// if the socket is currently flow-controlled.
int32_t Send(PP_Resource transport,
             const void* data,
             uint32_t len,
             PP_CompletionCallback cb) {
  // TODO(juberti): implement me
  return 0;
}


// Disconnects from the remote peer.
int32_t Close(PP_Resource transport) {
  // TODO(juberti): implement me
  return 0;
}


const PPB_Transport_Dev ppb_transport = {
  &CreateTransport,
  &IsTransport,
  &IsWritable,
  &Connect,
  &GetNextAddress,
  &ReceiveRemoteAddress,
  &Recv,
  &Send,
  &Close,
};

}  // namespace

PPB_Transport_Impl::PPB_Transport_Impl(PluginInstance* instance)
    : Resource(instance) {
  // TODO(juberti): impl
}

const PPB_Transport_Dev* PPB_Transport_Impl::GetInterface() {
  return &ppb_transport;
}

PPB_Transport_Impl::~PPB_Transport_Impl() {
  // TODO(juberti): teardown
}

PPB_Transport_Impl* PPB_Transport_Impl::AsPPB_Transport_Impl() {
  return this;
}

bool PPB_Transport_Impl::Init(const char* name, const char* proto) {
  // TODO(juberti): impl
  return false;
}

}  // namespace ppapi
}  // namespace webkit

