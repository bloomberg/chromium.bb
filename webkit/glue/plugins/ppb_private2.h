// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_

#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_point.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_var.h"

#define PPB_PRIVATE2_INTERFACE "PPB_Private2;2"

struct PP_FontDescription_Dev;

struct PPB_Private2 {
  // Sets or clears the rendering hint that the given plugin instance is always
  // on top of page content. Somewhat more optimized painting can be used in
  // this case.
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, bool on_top);

  bool (*DrawGlyphs)(PP_Resource pp_image_data,
                     const PP_FontDescription_Dev* font_desc,
                     uint32_t color,
                     PP_Point position,
                     PP_Rect clip,
                     float transformation[3][3],
                     uint32_t glyph_count,
                     uint16_t glyph_indices[],
                     PP_Point glyph_advances[]);

  // Retrieves the proxy that will be used for the given URL. The result will
  // be a string in PAC format, or an undefined var on error.
  PP_Var (*GetProxyForURL)(PP_Module module, const char* url);
};

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
