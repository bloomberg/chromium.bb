/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_INTERFACE "PPB_Flash;11"

/**
 * @file
 * This file contains the <code>PPB_Flash</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Flash</code> interface contains pointers to various functions
 * that are only needed to support Pepper Flash.
 */
struct PPB_Flash {
  /**
   * Sets or clears the rendering hint that the given plugin instance is always
   * on top of page content. Somewhat more optimized painting can be used in
   * this case.
   */
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
  /**
   * Retrieves the proxy that will be used for the given URL. The result will
   * be a string in PAC format, or an undefined var on error.
   */
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);
  /**
   * Navigate to the URL given by the given URLRequestInfo. (This supports GETs,
   * POSTs, and javascript: URLs.) May open a new tab if target is not "_self".
   */
  int32_t (*Navigate)(PP_Resource request_info,
                      const char* target,
                      bool from_user_action);
  /**
   * Runs a nested message loop. The plugin will be reentered from this call.
   * This function is used in places where Flash would normally enter a nested
   * message loop (e.g., when displaying context menus), but Pepper provides
   * only an asynchronous call. After performing that asynchronous call, call
   * |RunMessageLoop()|. In the callback, call |QuitMessageLoop()|.
   */
  void (*RunMessageLoop)(PP_Instance instance);
  /**
   * Posts a quit message for the outermost nested message loop. Use this to
   * exit and return back to the caller after you call RunMessageLoop.
   */
  void (*QuitMessageLoop)(PP_Instance instance);
  /**
   * Retrieves the local time zone offset from GM time for the given UTC time.
   */
  double (*GetLocalTimeZoneOffset)(PP_Instance instance, PP_Time t);
  /**
   * Gets a (string) with "command-line" options for Flash; used to pass
   * run-time debugging parameters, etc.
   */
  struct PP_Var (*GetCommandLineArgs)(PP_Module module);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_H_ */
