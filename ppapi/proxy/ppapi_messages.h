// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#define PPAPI_PROXY_PPAPI_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_param_traits.h"

namespace pp {
namespace proxy {

// PP_FontDescript_Dev has to be redefined with a SerializedVar in place of
// the PP_Var used for the face name.
struct SerializedFontDescription {
  pp::proxy::SerializedVar face;
  int32_t family;
  uint32_t size;
  int32_t weight;
  PP_Bool italic;
  PP_Bool small_caps;
  int32_t letter_spacing;
  int32_t word_spacing;
};

struct PPBFont_DrawTextAt_Params {
  PP_Resource font;
  PP_Resource image_data;
  pp::proxy::SerializedVar text;
  PP_Bool text_is_rtl;
  PP_Bool override_direction;
  PP_Point position;
  uint32_t color;
  PP_Rect clip;
  bool clip_is_null;
  PP_Bool image_data_is_opaque;
};

}  // namespace proxy
}  // namespace pp

#define MESSAGES_INTERNAL_FILE "ppapi/proxy/ppapi_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // PPAPI_PROXY_PPAPI_MESSAGES_H_
