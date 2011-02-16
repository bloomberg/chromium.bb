/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_INSTANCE_H_
#define PPAPI_C_PPB_INSTANCE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_INSTANCE_INTERFACE "PPB_Instance;0.4"

/**
 * @file
 * This file defines the PPB_Instance interface implemented by the
 * browser and containing pointers to functions related to
 * the module instance on a web page.
 *
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_Instance interface contains pointers to functions
 * related to the module instance on a web page.
 *
 */
struct PPB_Instance {
  /**
   * GetWindowObject is a pointer to a function that determines
   * the DOM window containing this module instance.
   *
   * @param[in] instance A PP_Instance whose WindowObject should be retrieved.
   * @return A PP_Var containing window object on success.
   */
  struct PP_Var (*GetWindowObject)(PP_Instance instance);

  /**
   * GetOwnerElementObject is a pointer to a function that determines
   * the DOM element containing this module instance.
   *
   * @param[in] instance A PP_Instance whose WindowObject should be retrieved.
   * @return A PP_Var containing DOM element on success.
   */
  struct PP_Var (*GetOwnerElementObject)(PP_Instance instance);

  /**
   * BindGraphics is a pointer to a function that binds the given
   * graphics as the current drawing surface. The
   * contents of this device is what will be displayed in the plugin's area
   * on the web page. The device must be a 2D or a 3D device.
   *
   * You can pass a NULL resource as the device parameter to unbind all
   * devices from the given instance. The instance will then appear
   * transparent. Re-binding the same device will return PP_TRUE and will do
   * nothing. Unbinding a device will drop any pending flush callbacks.
   *
   * Any previously-bound device will be Release()d. It is an error to bind
   * a device when it is already bound to another plugin instance. If you want
   * to move a device between instances, first unbind it from the old one, and
   * then rebind it to the new one.
   *
   * Binding a device will invalidate that portion of the web page to flush the
   * contents of the new device to the screen.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @param[in] device A PP_Resourse representing the graphics device.
   * @return PP_Bool containing PP_TRUE if bind was successful or PP_FALSE if
   * the device was not the correct type. On success, a reference to the
   * device will be held by the plugin instance, so the caller can release
   * its reference if it chooses.
   */
  PP_Bool (*BindGraphics)(PP_Instance instance, PP_Resource device);

  /**
   * IsFullFrame is a pointer to a function that determines if the
   * module instance is full-frame (repr). Such a module represents
   * the entire document in a frame rather than an embedded resource. This can
   * happen if the user does a top level navigation or the page specifies an
   * iframe to a resource with a MIME type registered by the plugin.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @return A PP_Bool containing PP_TRUE if the instance is full-frame.
   */
  PP_Bool (*IsFullFrame)(PP_Instance instance);

  /**
   * ExecuteScript is a pointer to a function that executes the given
   * script in the context of the frame containing the module.
   *
   * The exception, if any, will be returned in *exception. As with the PPB_Var
   * interface, the exception parameter, if non-NULL, must be initialized
   * to a void exception or the function will immediately return. On success,
   * the exception parameter will be set to a "void" var. On failure, the
   * return value will be a "void" var.
   *
   * @param[in] script A string containing the JavaScript to execute.
   * @param[in/out] exception PP_Var containing the exception. Initialize
   * this to NULL if you don't want exception info; initialize this to a void
   * exception if want exception info.
   *
   * @return The result of the script execution, or a "void" var
   * if execution failed.
   */
  struct PP_Var (*ExecuteScript)(PP_Instance instance,
                                 struct PP_Var script,
                                 struct PP_Var* exception);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_INSTANCE_H_ */

