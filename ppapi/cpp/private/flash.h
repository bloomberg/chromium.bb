// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_FLASH_H_
#define PPAPI_CPP_PRIVATE_FLASH_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

struct PP_Point;

namespace pp {

class FontDescription_Dev;
class ImageData;
class Instance;
class Module;
class Point;
class Rect;
class URLRequestInfo;
class Var;

namespace flash {

class Flash {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  static void SetInstanceAlwaysOnTop(Instance* instance, bool on_top);
  static bool DrawGlyphs(Instance* instance,
                         ImageData* image,
                         const FontDescription_Dev& font_desc,
                         uint32_t color,
                         const Point& position,
                         const Rect& clip,
                         const float transformation[3][3],
                         bool allow_subpixel_aa,
                         uint32_t glyph_count,
                         const uint16_t glyph_indices[],
                         const PP_Point glyph_advances[]);
  static Var GetProxyForURL(Instance* instance, const std::string& url);
  static int32_t Navigate(const URLRequestInfo& request_info,
                          const std::string& target,
                          bool from_user_action);
  static void RunMessageLoop(Instance* instance);
  static void QuitMessageLoop(Instance* instance);
  static double GetLocalTimeZoneOffset(Instance* instance, PP_Time t);
  static Var GetCommandLineArgs(Module* module);
  static void PreloadFontWin(const void* logfontw);
};

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_H_
