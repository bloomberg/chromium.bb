// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_transport_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Transport_API> EnterTransport;

PP_Resource Create(PP_Instance instance, const char* name,
                   PP_TransportType type) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTransport(instance, name, type);
}

PP_Bool IsTransport(PP_Resource resource) {
  EnterTransport enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool IsWritable(PP_Resource transport) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsWritable();
}

int32_t SetProperty(PP_Resource transport, PP_TransportProperty property,
                   PP_Var value) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->SetProperty(property, value);
}

int32_t Connect(PP_Resource transport, PP_CompletionCallback callback) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Connect(callback);
  return MayForceCallback(callback, result);
}

int32_t GetNextAddress(PP_Resource transport, PP_Var* address,
                       PP_CompletionCallback callback) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->GetNextAddress(address, callback);
  return MayForceCallback(callback, result);
}

int32_t ReceiveRemoteAddress(PP_Resource transport, PP_Var address) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->ReceiveRemoteAddress(address);
}

int32_t Recv(PP_Resource transport, void* data, uint32_t len,
             PP_CompletionCallback callback) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Recv(data, len, callback);
  return MayForceCallback(callback, result);
}

int32_t Send(PP_Resource transport, const void* data, uint32_t len,
             PP_CompletionCallback callback) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Send(data, len, callback);
  return MayForceCallback(callback, result);
}

int32_t Close(PP_Resource transport) {
  EnterTransport enter(transport, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->Close();
}

const PPB_Transport_Dev g_ppb_transport_thunk = {
  &Create,
  &IsTransport,
  &IsWritable,
  &SetProperty,
  &Connect,
  &GetNextAddress,
  &ReceiveRemoteAddress,
  &Recv,
  &Send,
  &Close,
};

}  // namespace

const PPB_Transport_Dev* GetPPB_Transport_Dev_Thunk() {
  return &g_ppb_transport_thunk;
}

}  // namespace thunk
}  // namespace ppapi
