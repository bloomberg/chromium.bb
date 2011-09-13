// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

void Log(PP_Instance instance, PP_LogLevel_Dev level, struct PP_Var value) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    return enter.functions()->Log(instance, level, value);
}

void LogWithSource(PP_Instance instance,
                   PP_LogLevel_Dev level,
                   PP_Var source,
                   PP_Var value) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    return enter.functions()->LogWithSource(instance, level, source, value);
}

const PPB_Console_Dev g_ppb_console_thunk = {
  &Log,
  &LogWithSource
};

}  // namespace

const PPB_Console_Dev* GetPPB_Console_Dev_Thunk() {
  return &g_ppb_console_thunk;
}

}  // namespace thunk
}  // namespace ppapi
