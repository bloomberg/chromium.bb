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
 * Defines the API ...
 */

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * A resource is data associated with the Pepper plugin interface. While a
 * Var represents something callable to JS or from the plugin to the DOM, a
 * resource has no meaning or visibility outside of the plugin interface.
 *
 * Resources are reference counted. Use AddRefResource and ReleaseResource to
 * manage your reference count of a resource. The data will be automatically
 * destroyed when the internal reference count reaches 0.
 *
 * Value is an opaque handle assigned by the browser to the resource. It is
 * guaranteed never to be 0 for a valid resource, so a plugin can initialize
 * it to 0 to indicate a "NULL handle." Some interfaces may return a NULL
 * resource to indicate failure.
 *
 */
typedef int32_t PP_Resource;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Resource, 4);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_RESOURCE_H_ */

