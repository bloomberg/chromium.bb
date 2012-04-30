// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/ppb_messaging.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

void PostMessage(PP_Instance instance, PP_Var message) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->PostMessage(instance, message);
}

const PPB_Messaging g_ppb_messaging_thunk = {
  &PostMessage
};

}  // namespace

const PPB_Messaging_1_0* GetPPB_Messaging_1_0_Thunk() {
  return &g_ppb_messaging_thunk;
}

}  // namespace thunk
}  // namespace ppapi
