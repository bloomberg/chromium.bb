// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool GetDefaultPrintSettings(PP_Instance instance,
                                PP_PrintSettings_Dev* print_settings) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->GetDefaultPrintSettings(instance, print_settings);
}

const PPB_Printing_Dev g_ppb_printing_dev_thunk = {
  &GetDefaultPrintSettings,
};

}  // namespace

const PPB_Printing_Dev_0_6* GetPPB_Printing_Dev_0_6_Thunk() {
  return &g_ppb_printing_dev_thunk;
}

}  // namespace thunk
}  // namespace ppapi
