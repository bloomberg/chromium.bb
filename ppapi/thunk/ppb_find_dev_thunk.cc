// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_find_dev.idl modified Tue Aug 20 08:13:36 2013.

#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void NumberOfFindResultsChanged(PP_Instance instance,
                                int32_t total,
                                PP_Bool final_result) {
  VLOG(4) << "PPB_Find_Dev::NumberOfFindResultsChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->NumberOfFindResultsChanged(instance, total, final_result);
}

void SelectedFindResultChanged(PP_Instance instance, int32_t index) {
  VLOG(4) << "PPB_Find_Dev::SelectedFindResultChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SelectedFindResultChanged(instance, index);
}

const PPB_Find_Dev_0_3 g_ppb_find_dev_thunk_0_3 = {
  &NumberOfFindResultsChanged,
  &SelectedFindResultChanged
};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Find_Dev_0_3* GetPPB_Find_Dev_0_3_Thunk() {
  return &g_ppb_find_dev_thunk_0_3;
}

}  // namespace thunk
}  // namespace ppapi
