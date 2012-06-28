/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_graphics_2d_dev.idl modified Tue Jun 19 14:11:08 2012. */

#ifndef PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_
#define PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GRAPHICS2D_DEV_INTERFACE_0_1 "PPB_Graphics2D(Dev);0.1"
#define PPB_GRAPHICS2D_DEV_INTERFACE PPB_GRAPHICS2D_DEV_INTERFACE_0_1

/**
 * @file
 * This file contains the <code>PPB_Graphics2D_Dev</code> interface. */


/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_Graphics2D_Dev interface */
struct PPB_Graphics2D_Dev_0_1 {
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
};

typedef struct PPB_Graphics2D_Dev_0_1 PPB_Graphics2D_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_GRAPHICS_2D_DEV_H_ */

