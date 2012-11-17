// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ../test_thunk/simple.idl modified Fri Nov 16 11:26:06 2012.

#include "ppapi/c/../test_thunk/simple.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/simple_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateSimple(instance);
}

PP_Bool IsSimple(PP_Resource resource) {
  EnterResource<PPB_Simple_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

void PostMessage(PP_Instance instance, PP_Var message) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->PostMessage(instance, message);
}

uint32_t DoUint32Instance(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->DoUint32Instance(instance);
}

uint32_t DoUint32Resource(PP_Resource instance) {
  EnterResource<PPB_Simple_API> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.object()->DoUint32Resource();
}

uint32_t DoUint32ResourceNoErrors(PP_Resource instance) {
  EnterResource<PPB_Simple_API> enter(instance, false);
  if (enter.failed())
    return 0;
  return enter.object()->DoUint32ResourceNoErrors();
}

int32_t OnFailure12(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return 12;
  return enter.functions()->OnFailure12(instance);
}

const PPB_Simple_0_5 g_ppb_simple_thunk_0_5 = {
  &Create,
  &IsSimple,
  &PostMessage,
  &DoUint32Instance,
  &DoUint32Resource,
  &DoUint32ResourceNoErrors,
};

const PPB_Simple_1_0 g_ppb_simple_thunk_1_0 = {
  &Create,
  &IsSimple,
  &DoUint32Instance,
  &DoUint32Resource,
  &DoUint32ResourceNoErrors,
  &OnFailure12,
};

}  // namespace

const PPB_Simple_0_5* GetPPB_Simple_0_5_Thunk() {
  return &g_ppb_simple_thunk_0_5;
}

const PPB_Simple_1_0* GetPPB_Simple_1_0_Thunk() {
  return &g_ppb_simple_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
