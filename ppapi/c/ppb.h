/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_H_
#define PPAPI_C_PPB_H_

/**
 * @file
 * Defines the API ...
 *
 */

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * Returns an interface pointer for the interface of the given name, or NULL
 * if the interface is not supported. Interface names should be ASCII.
 */
typedef const void* (*PPB_GetInterface)(const char* interface_name);
/**
 * @}
 */
#endif  /* PPAPI_C_PPB_H_ */

