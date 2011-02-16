/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_H_
#define PPAPI_C_PPB_H_

/**
 * @file
 * This file defines a function pointer type for the PPB_GetInterface function.
 */

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * This function pointer type defines the signature for the PPB_GetInterface
 * function. A generic PPB_GetInterface pointer is passed to
 * PPP_InitializedModule when your module is loaded. You can use this pointer
 * to request a pointer to a specific browser interface. Browser interface
 * names are ASCII strings and are generally defined in the header file for the
 * interface, such as PP_AUDIO_INTERFACE found in ppb.audio.h or
 * PPB_GRAPHICS_2D_INTERFACE in ppb_graphics_2d.h.
 *
 * This value will be NULL if the interface is not supported on the browser.
 */
typedef const void* (*PPB_GetInterface)(const char* interface_name);
/**
 * @}
 */
#endif  /* PPAPI_C_PPB_H_ */

