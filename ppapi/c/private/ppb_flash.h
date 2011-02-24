// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_INTERFACE "PPB_Flash;7"

struct PPB_Flash {
  // Sets or clears the rendering hint that the given plugin instance is always
  // on top of page content. Somewhat more optimized painting can be used in
  // this case.
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);

  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        struct PP_Point position,
                        struct PP_Rect clip,
                        const float transformation[3][3],
                        uint32_t glyph_count,
                        const uint16_t glyph_indices[],
                        const struct PP_Point glyph_advances[]);

  // Retrieves the proxy that will be used for the given URL. The result will
  // be a string in PAC format, or an undefined var on error.
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);

  // Navigate to URL. May open a new tab if target is not "_self". Return true
  // if success. This differs from javascript:window.open() in that it bypasses
  // the popup blocker, even when this is not called from an event handler.
  PP_Bool (*NavigateToURL)(PP_Instance instance,
                           const char* url,
                           const char* target);

  // Runs a nested message loop. The plugin will be reentered from this call.
  // This function is used in places where Flash would normally enter a nested
  // message loop (e.g., when displaying context menus), but Pepper provides
  // only an asynchronous call. After performing that asynchronous call, call
  // |RunMessageLoop()|. In the callback, call |QuitMessageLoop()|.
  void (*RunMessageLoop)(PP_Instance instance);

  // Posts a quit message for the outermost nested message loop. Use this to
  // exit and return back to the caller after you call RunMessageLoop.
  void (*QuitMessageLoop)(PP_Instance instance);
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_H_
