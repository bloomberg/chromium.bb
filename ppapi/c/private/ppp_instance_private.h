/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_

#include "ppapi/c/pp_instance.h"

struct PP_Var;

#define PPP_INSTANCE_PRIVATE_INTERFACE "PPP_Instance_Private;0.1"

/**
 * @file
 * This file defines the PPP_InstancePrivate structure; a series of functions
 * that a trusted plugin may implement to provide capabilities only available
 * to trusted plugins.
 *
 */

/** @addtogroup Interfaces
 * @{
 */

/**
 * The PPP_Instance_Private interface contains pointers to a series of
 * functions that may be implemented in a trusted plugin to provide capabilities
 * that aren't possible in untrusted modules.
 */

struct PPP_Instance_Private {
  /**
   * GetInstanceObject returns a PP_Var representing the scriptable object for
   * the given instance. Normally this will be a PPP_Class_Deprecated object
   * that exposes methods and properties to JavaScript.
   *
   * On Failure, the returned PP_Var should be a "void" var.
   *
   * The returned PP_Var should have a reference added for the caller, which
   * will be responsible for Release()ing that reference.
   *
   * @param[in] instance A PP_Instance indentifying the instance from which the
   *            instance object is being requested.
   * @return A PP_Var containing scriptable object.
   */
  struct PP_Var (*GetInstanceObject)(PP_Instance instance);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_INSTANCE_PRIVATE_H_ */

