/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_RESOURCE_H_
#define PPAPI_C_PP_RESOURCE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the PP_Resource type which represents data associated with
 * the module.
 */

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * This typdef represents an opaque handle assigned by the browser to the
 * resource. The handle is guaranteed never to be 0 for a valid resource, so a
 * module can initialize it to 0 to indicate a "NULL handle." Some interfaces
 * may return a NULL resource to indicate failure.
 *
 * While a Var represents something callable to JS or from the module to
 * the DOM, a resource has no meaning or visibility outside of the module
 * interface.
 *
 * Resources are reference counted. Use AddRefResource and ReleaseResource in
 * ppb_core.h to manage the reference count of a resource. The data will be
 * automatically destroyed when the internal reference count reaches 0.
 */
typedef int32_t PP_Resource;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Resource, 4);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_RESOURCE_H_ */

