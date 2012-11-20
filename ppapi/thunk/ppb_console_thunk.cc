// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_console_dev.idl modified Mon Nov 19 09:22:44 2012.

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void Log(PP_Instance instance, PP_LogLevel_Dev level, struct PP_Var value) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->Log(instance, level, value);
}

void LogWithSource(PP_Instance instance,
                   PP_LogLevel_Dev level,
                   struct PP_Var source,
                   struct PP_Var value) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->LogWithSource(instance, level, source, value);
}

const PPB_Console_Dev_0_1 g_ppb_console_dev_thunk_0_1 = {
  &Log,
  &LogWithSource,
};

}  // namespace

const PPB_Console_Dev_0_1* GetPPB_Console_Dev_0_1_Thunk() {
  return &g_ppb_console_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
