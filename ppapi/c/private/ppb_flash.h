/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_flash.idl modified Tue May 01 16:01:19 2012. */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_H_

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_INTERFACE_12_0 "PPB_Flash;12.0"
#define PPB_FLASH_INTERFACE_12_1 "PPB_Flash;12.1"
#define PPB_FLASH_INTERFACE_12_2 "PPB_Flash;12.2"
#define PPB_FLASH_INTERFACE_12_3 "PPB_Flash;12.3"
#define PPB_FLASH_INTERFACE PPB_FLASH_INTERFACE_12_3

/**
 * @file
 * This file contains the <code>PPB_Flash</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /**
   * Specifies if the system likely supports 3D hardware acceleration.
   *
   * The result is an int where 1 corresponds to true and 0 corresponds to
   * false, depending on the supported nature of 3D acceleration. If querying
   * this function returns 1, the 3D system will normally use the native
   * hardware for rendering which will be much faster.
   *
   * In rare cases (depending on the platform) this value will be 1 but a
   * created 3D context will use emulation because context initialization
   * failed.
   */
  PP_FLASHSETTING_3DENABLED = 1,
  /**
   * Specifies if the given instance is in private/inconito/off-the-record mode
   * (returns 1) or "regular" mode (returns 0). Returns -1 on invalid instance.
   */
  PP_FLASHSETTING_INCOGNITO = 2
} PP_FlashSetting;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FlashSetting, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Flash</code> interface contains pointers to various functions
 * that are only needed to support Pepper Flash.
 */
struct PPB_Flash_12_3 {
  /**
   * Sets or clears the rendering hint that the given plugin instance is always
   * on top of page content. Somewhat more optimized painting can be used in
   * this case.
   */
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);
  /**
   * Draws the given pre-laid-out text. It is almost equivalent to Windows'
   * ExtTextOut with the addition of the transformation (a 3x3 matrix given the
   * transform to apply before drawing). It also adds the allow_subpixel_aa
   * flag which when true, will use subpixel antialiasing if enabled in the
   * system settings. For this to work properly, the graphics layer that the
   * text is being drawn into must be opaque.
   */
  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        const struct PP_Point* position,
                        const struct PP_Rect* clip,
                        const float transformation[3][3],
                        PP_Bool allow_subpixel_aa,
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
                      PP_Bool from_user_action);
  /**
   * Runs a nested message loop. The plugin will be reentered from this call.
   * This function is used in places where Flash would normally enter a nested
   * message loop (e.g., when displaying context menus), but Pepper provides
   * only an asynchronous call. After performing that asynchronous call, call
   * |RunMessageLoop()|. In the callback, call |QuitMessageLoop()|.
   */
  void (*RunMessageLoop)(PP_Instance instance);
  /* Posts a quit message for the outermost nested message loop. Use this to
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
  /**
   * Loads the given font in a more priviledged process on Windows. Call this if
   * Windows is giving errors for font calls. See
   * content/renderer/font_cache_dispatcher_win.cc
   *
   * The parameter is a pointer to a LOGFONTW structure.
   *
   * On non-Windows platforms, this function does nothing.
   */
  void (*PreloadFontWin)(const void* logfontw);
  /**
   * Returns whether the given rectangle (in the plugin) is topmost, i.e., above
   * all other web content.
   */
  PP_Bool (*IsRectTopmost)(PP_Instance instance, const struct PP_Rect* rect);
  /**
   * Does nothing, deprecated. See PPB_Flash_Print.
   */
  int32_t (*InvokePrinting)(PP_Instance instance);
  /**
   * Indicates that there's activity and, e.g., the screensaver shouldn't kick
   * in.
   */
  void (*UpdateActivity)(PP_Instance instance);
  /**
   * Returns the device ID as a string. Returns a PP_VARTYPE_UNDEFINED on error.
   */
  struct PP_Var (*GetDeviceID)(PP_Instance instance);
  /**
   * Returns the value associated with the given setting. Invalid enums will
   * result in -1 return value.
   */
  int32_t (*GetSettingInt)(PP_Instance instance, PP_FlashSetting setting);
};

typedef struct PPB_Flash_12_3 PPB_Flash;

struct PPB_Flash_12_0 {
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);
  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        const struct PP_Point* position,
                        const struct PP_Rect* clip,
                        const float transformation[3][3],
                        PP_Bool allow_subpixel_aa,
                        uint32_t glyph_count,
                        const uint16_t glyph_indices[],
                        const struct PP_Point glyph_advances[]);
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);
  int32_t (*Navigate)(PP_Resource request_info,
                      const char* target,
                      PP_Bool from_user_action);
  void (*RunMessageLoop)(PP_Instance instance);
  void (*QuitMessageLoop)(PP_Instance instance);
  double (*GetLocalTimeZoneOffset)(PP_Instance instance, PP_Time t);
  struct PP_Var (*GetCommandLineArgs)(PP_Module module);
  void (*PreloadFontWin)(const void* logfontw);
};

struct PPB_Flash_12_1 {
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);
  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        const struct PP_Point* position,
                        const struct PP_Rect* clip,
                        const float transformation[3][3],
                        PP_Bool allow_subpixel_aa,
                        uint32_t glyph_count,
                        const uint16_t glyph_indices[],
                        const struct PP_Point glyph_advances[]);
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);
  int32_t (*Navigate)(PP_Resource request_info,
                      const char* target,
                      PP_Bool from_user_action);
  void (*RunMessageLoop)(PP_Instance instance);
  void (*QuitMessageLoop)(PP_Instance instance);
  double (*GetLocalTimeZoneOffset)(PP_Instance instance, PP_Time t);
  struct PP_Var (*GetCommandLineArgs)(PP_Module module);
  void (*PreloadFontWin)(const void* logfontw);
  PP_Bool (*IsRectTopmost)(PP_Instance instance, const struct PP_Rect* rect);
  int32_t (*InvokePrinting)(PP_Instance instance);
  void (*UpdateActivity)(PP_Instance instance);
};

struct PPB_Flash_12_2 {
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);
  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        const struct PP_Point* position,
                        const struct PP_Rect* clip,
                        const float transformation[3][3],
                        PP_Bool allow_subpixel_aa,
                        uint32_t glyph_count,
                        const uint16_t glyph_indices[],
                        const struct PP_Point glyph_advances[]);
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);
  int32_t (*Navigate)(PP_Resource request_info,
                      const char* target,
                      PP_Bool from_user_action);
  void (*RunMessageLoop)(PP_Instance instance);
  void (*QuitMessageLoop)(PP_Instance instance);
  double (*GetLocalTimeZoneOffset)(PP_Instance instance, PP_Time t);
  struct PP_Var (*GetCommandLineArgs)(PP_Module module);
  void (*PreloadFontWin)(const void* logfontw);
  PP_Bool (*IsRectTopmost)(PP_Instance instance, const struct PP_Rect* rect);
  int32_t (*InvokePrinting)(PP_Instance instance);
  void (*UpdateActivity)(PP_Instance instance);
  struct PP_Var (*GetDeviceID)(PP_Instance instance);
};
/**
 * @}
 */

/**
 * The old version of the interface, which cannot be generated from IDL.
 * TODO(viettrungluu): Remove this when enough time has passed. crbug.com/104184
 */
#define PPB_FLASH_INTERFACE_11_0 "PPB_Flash;11"
struct PPB_Flash_11 {
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
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);
  int32_t (*Navigate)(PP_Resource request_info,
                      const char* target,
                      bool from_user_action);
  void (*RunMessageLoop)(PP_Instance instance);
  void (*QuitMessageLoop)(PP_Instance instance);
  double (*GetLocalTimeZoneOffset)(PP_Instance instance, PP_Time t);
  struct PP_Var (*GetCommandLineArgs)(PP_Module module);
};
#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_H_ */

