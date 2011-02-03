/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPP_H_
#define PPAPI_C_PPP_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb.h"

#if __GNUC__ >= 4
#define PP_EXPORT __attribute__ ((visibility("default")))
#elif defined(_MSC_VER)
#define PP_EXPORT __declspec(dllexport)
#endif

/**
 * @file
 * This file defines three functions that your module must
 * implement to interact with the browser.
 *
 * {PENDING: undefine PP_EXPORT?}
 */

/* We don't want name mangling for these external functions.  We only need
 * 'extern "C"' if we're compiling with a C++ compiler.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_InitializeModule() is the entry point for a Native Client module and is
 * called by the browser when your module loads. Your code must implement this
 * function.
 *
 * Failure indicates to the browser that this plugin can not be used. In this
 * case, the plugin will be unloaded and ShutdownModule will NOT be called.
 *
 * @param[in] module A handle to one Native Client module.
 * @param[in] get_browser_interface An interface pointer.
 * @return PP_OK on success. Any other value on failure.
*/
PP_EXPORT int32_t PPP_InitializeModule(PP_Module module,
                                       PPB_GetInterface get_browser_interface);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/** PPP_ShutdownModule() is called before the Native Client module is unloaded.
 * Your code must implement this function.
 */
PP_EXPORT void PPP_ShutdownModule();
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_GetInterface() is called by the browser to determine the PPP_Instance
 * functions that the Native Client module implements. PPP_Instance is
 * an interface (struct) that contains pointers to several functions your
 * module must implement in some form (all functions can be empty, but
 * must be implemented). If you care about things such as keyboard events
 * or your module gaining or losing focus on a page, these functions must
 * have code to handle those events. Refer to PPP_Instance interface for
 * more information on these functions.
 *
 * @param[in] interface_name A pointer to an interface name. Interface names
 * should be ASCII.
 * @return An interface pointer for the interface or NULL if the interface is
 * not supported.
 */
PP_EXPORT const void* PPP_GetInterface(const char* interface_name);
/**
 * @}
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* PPAPI_C_PPP_H_ */

