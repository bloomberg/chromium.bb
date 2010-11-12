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

struct SerializedDirEntry {
  std::string name;
  bool is_dir;
};

// Since there are so many parameters, DrawTextAt requires this separate
// structure. This includes everything but the font name. Because the font name
// is a var, it's much more convenient to use the normal way of passing a
// PP_Var.
struct PPBFont_DrawTextAt_Params {
  PP_Resource font;
  PP_Resource image_data;
  PP_Bool text_is_rtl;
  PP_Bool override_direction;
  PP_Point position;
  uint32_t color;
  PP_Rect clip;
  bool clip_is_null;
  PP_Bool image_data_is_opaque;
};

struct PPBFlash_DrawGlyphs_Params {
  PP_Resource pp_image_data;
  SerializedFontDescription font_desc;
  uint32_t color;
  PP_Point position;
  PP_Rect clip;
  float transformation[3][3];
  std::vector<uint16_t> glyph_indices;
  std::vector<PP_Point> glyph_advances;
};

}  // namespace proxy
}  // namespace pp

#define MESSAGES_INTERNAL_FILE "ppapi/proxy/ppapi_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // PPAPI_PROXY_PPAPI_MESSAGES_H_
