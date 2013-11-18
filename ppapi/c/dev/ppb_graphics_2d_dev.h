/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_graphics_2d_dev.idl modified Tue Nov 12 11:22:26 2013. */

#ifndef PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_
#define PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GRAPHICS2D_DEV_INTERFACE_0_1 "PPB_Graphics2D(Dev);0.1"
#define PPB_GRAPHICS2D_DEV_INTERFACE_0_2 "PPB_Graphics2D(Dev);0.2"
#define PPB_GRAPHICS2D_DEV_INTERFACE PPB_GRAPHICS2D_DEV_INTERFACE_0_2

/**
 * @file
 * This file contains the <code>PPB_Graphics2D_Dev</code> interface. */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * These options affect how the existing graphics context is displayed when a
 * plugin is resized.
 */
typedef enum {
  /**
   * In this mode, the context does not change size or offset. If the backing
   * store is the same size as the plugin element, this will result in the
   * pixels on the right side of the plugin element being unavailable, revealing
   * the contents underneath it.
   */
  PP_GRAPHICS2D_DEV_RESIZEMODE_DEFAULT,
  /**
   * In this mode, the context and its offset are scaled relative to how much
   * the plugin element has been resized.
   */
  PP_GRAPHICS2D_DEV_RESIZEMODE_STRETCH
} PP_Graphics2D_Dev_ResizeMode;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Graphics2D_Dev_ResizeMode, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_Graphics2D_Dev interface */
struct PPB_Graphics2D_Dev_0_2 {
  /**
   * SetScale() sets the scale factor that will be applied when painting the
   * graphics context onto the output device. Typically, if rendering at device
   * resolution is desired, the context would be created with the width and
   * height scaled up by the view's GetDeviceScale and SetScale called with a
   * scale of 1.0 / GetDeviceScale(). For example, if the view resource passed
   * to DidChangeView has a rectangle of (w=200, h=100) and a device scale of
   * 2.0, one would call Create with a size of (w=400, h=200) and then call
   * SetScale with 0.5. One would then treat each pixel in the context as a
   * single device pixel.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   * @param[in] scale The scale to apply when painting.
   *
   * @return Returns <code>PP_TRUE</code> on success or <code>PP_FALSE</code> if
   * the resource is invalid or the scale factor is 0 or less.
   */
  PP_Bool (*SetScale)(PP_Resource resource, float scale);
  /***
   * GetScale() gets the scale factor that will be applied when painting the
   * graphics context onto the output device.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   *
   * @return Returns the scale factor for the graphics context. If the resource
   * is not a valid <code>Graphics2D</code> context, this will return 0.0.
   */
  float (*GetScale)(PP_Resource resource);
  /***
   * Sets the offset into the plugin element at which the graphics context is
   * painted. This allows a portion of the plugin element to be painted to.
   * The new offset will only be applied after Flush() has been called.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   * @param[in] offset The offset at which the context should be painted.
   */
  void (*SetOffset)(PP_Resource resource, const struct PP_Point* offset);
  /***
   * Sets the resize mode for the graphics context. When a plugin element is
   * resized in the DOM, it takes time for the plugin to update the graphics
   * context in the renderer. These options affect how the existing context is
   * displayed until the backing store is updated by the plugin.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   * @param[in] resize_mode The resize mode to change this context to.
   */
  void (*SetResizeMode)(PP_Resource resource,
                        PP_Graphics2D_Dev_ResizeMode resize_mode);
};

typedef struct PPB_Graphics2D_Dev_0_2 PPB_Graphics2D_Dev;

struct PPB_Graphics2D_Dev_0_1 {
  PP_Bool (*SetScale)(PP_Resource resource, float scale);
  float (*GetScale)(PP_Resource resource);
  void (*SetResizeMode)(PP_Resource resource,
                        PP_Graphics2D_Dev_ResizeMode resize_mode);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_ */

