/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_instance.idl modified Mon Aug 15 11:01:06 2011. */

#ifndef PPAPI_C_PPB_INSTANCE_H_
#define PPAPI_C_PPB_INSTANCE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_INSTANCE_INTERFACE_0_5 "PPB_Instance;0.5"
#define PPB_INSTANCE_INTERFACE_1_0 "PPB_Instance;1.0"
#define PPB_INSTANCE_INTERFACE PPB_INSTANCE_INTERFACE_1_0

/**
 * @file
 * This file defines the PPB_Instance interface implemented by the
 * browser and containing pointers to functions related to
 * the module instance on a web page.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The PPB_Instance interface contains pointers to functions
 * related to the module instance on a web page.
 */
struct PPB_Instance {
  /**
   * BindGraphics() binds the given graphics as the current display surface.
   * The contents of this device is what will be displayed in the instance's
   * area on the web page. The device must be a 2D or a 3D device.
   *
   * You can pass a <code>NULL</code> resource as the device parameter to
   * unbind all devices from the given instance. The instance will then appear
   * transparent. Re-binding the same device will return <code>PP_TRUE</code>
   * and will do nothing.
   *
   * Any previously-bound device will be released. It is an error to bind
   * a device when it is already bound to another instance. If you want
   * to move a device between instances, first unbind it from the old one, and
   * then rebind it to the new one.
   *
   * Binding a device will invalidate that portion of the web page to flush the
   * contents of the new device to the screen.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @param[in] device A PP_Resource representing the graphics device.
   *
   * @return <code>PP_Bool</code> containing <code>PP_TRUE</code> if bind was
   * successful or <code>PP_FALSE</code> if the device was not the correct
   * type. On success, a reference to the device will be held by the
   * instance, so the caller can release its reference if it chooses.
   */
  PP_Bool (*BindGraphics)(PP_Instance instance, PP_Resource device);
  /**
   * IsFullFrame() determines if the instance is full-frame. Such an instance
   * represents the entire document in a frame rather than an embedded
   * resource. This can happen if the user does a top-level navigation or the
   * page specifies an iframe to a resource with a MIME type registered by the
   * module.
   *
   * @param[in] instance A <code>PP_Instance</code> indentifying one instance
   * of a module.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if the
   * instance is full-frame.
   */
  PP_Bool (*IsFullFrame)(PP_Instance instance);
};

struct PPB_Instance_0_5 {
  struct PP_Var (*GetWindowObject)(PP_Instance instance);
  struct PP_Var (*GetOwnerElementObject)(PP_Instance instance);
  PP_Bool (*BindGraphics)(PP_Instance instance, PP_Resource device);
  PP_Bool (*IsFullFrame)(PP_Instance instance);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_INSTANCE_H_ */

