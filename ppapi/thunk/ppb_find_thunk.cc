// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace thunk {

namespace {

void NumberOfFindResultsChanged(PP_Instance instance,
                                int32_t total,
                                PP_Bool final) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->NumberOfFindResultsChanged(instance, total, final);
}

void SelectedFindResultChanged(PP_Instance instance, int32_t index) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SelectedFindResultChanged(instance, index);
}

const PPB_Find_Dev g_ppb_find_thunk = {
  &NumberOfFindResultsChanged,
  &SelectedFindResultChanged
};

}  // namespace

const PPB_Find_Dev* GetPPB_Find_Dev_Thunk() {
  return &g_ppb_find_thunk;
}

}  // namespace thunk
}  // namespace ppapi
