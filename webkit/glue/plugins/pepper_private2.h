// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_

#include "build/build_config.h"
#include "third_party/ppapi/c/pp_point.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PP_FontDescription_Dev;
struct PPB_Private2;

namespace pepper {

class Private2 {
 public:
  // Returns a pointer to the interface implementing PPB_Private2 that is
  // exposed to the plugin.
  static const PPB_Private2* GetInterface();

  static bool DrawGlyphs(PP_Resource pp_image_data,
                         const PP_FontDescription_Dev* font_desc,
                         uint32_t color,
                         PP_Point position,
                         PP_Rect clip,
                         float transformation[3][3],
                         uint32_t glyph_count,
                         uint16_t glyph_indices[],
                         PP_Point glyph_advances[])
#if defined(OS_LINUX)
      ;
#else
      { return false; }
#endif
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PRIVATE2_H_
