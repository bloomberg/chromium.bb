// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_broker_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource CreateTrusted(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateBroker(instance);
}

PP_Bool IsBrokerTrusted(PP_Resource resource) {
  EnterResource<PPB_Broker_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Connect(PP_Resource resource,
                PP_CompletionCallback connect_callback) {
  EnterResource<PPB_Broker_API> enter(resource, true);
  if (enter.failed())
    return MayForceCallback(connect_callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Connect(connect_callback);
  return MayForceCallback(connect_callback, result);
}

int32_t GetHandle(PP_Resource resource, int32_t* handle) {
  EnterResource<PPB_Broker_API> enter(resource, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetHandle(handle);
}

const PPB_BrokerTrusted g_ppb_broker_thunk = {
  &CreateTrusted,
  &IsBrokerTrusted,
  &Connect,
  &GetHandle,
};

}  // namespace

const PPB_BrokerTrusted* GetPPB_BrokerTrusted_Thunk() {
  return &g_ppb_broker_thunk;
}

}  // namespace thunk
}  // namespace ppapi
