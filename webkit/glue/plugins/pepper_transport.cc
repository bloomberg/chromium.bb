// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_transport.h"

#include "base/singleton.h"
#include "base/thread_local.h"
#include "third_party/ppapi/c/dev/ppb_transport_dev.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"

namespace pepper {

namespace {

// Creates a new transport object with the specified name
// using the specified protocol.
PP_Resource CreateTransport(PP_Module module,
                            const char* name,
                            const char* proto) {
  // TODO(juberti): implement me
  PP_Resource p(NULL);
  return p;
}

// Returns whether or not resource is Transport
bool IsTransport(PP_Resource resource) {
  return !!Resource::GetAs<Transport>(resource);
}

// Returns whether the transport is currently writable
// (i.e. can send data to the remote peer)
bool IsWritable(PP_Resource transport) {
  // TODO(juberti): impelement me
  return false;
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
  // TODO(juberti): impelement me
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

Transport::Transport(PluginModule* module)
    : Resource(module) {
  // TODO(juberti): impl
}

const PPB_Transport_Dev* Transport::GetInterface() {
  return &ppb_transport;
}

Transport::~Transport() {
  // TODO(juberti): teardown
}

bool Transport::Init(const char* name,
                     const char* proto) {
  // TODO(juberti): impl
  return false;
}

}  // namespace pepper

