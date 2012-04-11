// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool SetCursor(PP_Instance instance,
                  PP_MouseCursor_Type type,
                  PP_Resource image,
                  const PP_Point* hot_spot) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetCursor(instance, type, image, hot_spot);
}

const PPB_MouseCursor_1_0 g_ppb_mouse_cursor_thunk = {
  &SetCursor
};

}  // namespace

const PPB_MouseCursor_1_0* GetPPB_MouseCursor_1_0_Thunk() {
  return &g_ppb_mouse_cursor_thunk;
}

}  // namespace thunk
}  // namespace ppapi
