/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
 */

// {PENDING: undefine PP_EXPORT?}


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
 * PPP_InitializeModule() is the entry point for a module and is called by the
 * browser when your module loads. Your code must implement this function.
 *
 * Failure indicates to the browser that this plugin can not be used. In this
 * case, the plugin will be unloaded and ShutdownModule will NOT be called.
 *
 * @param[in] module A handle to your module. Generally you should store this
 * value since it will be required for other API calls.
 *
 * @param[in] get_browser_interface A pointer to the function that you can
 * use to query for browser interfaces. Generally you should store this value
 * for future use.
 *
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

/**
 * PPP_ShutdownModule() is <strong>sometimes</strong> called before the module
 * is unloaded. It is not recommended that you implement this function.
 *
 * There is no practical use of this function for third party plugins. Its
 * existence is because of some internal use cases inside Chrome.
 *
 * Since your plugin runs in a separate process, there's no need to free
 * allocated memory. There is also no need to free any resources since all of
 * resources associated with an instance will be force-freed when that instance
 * is deleted. Moreover, this function will not be called when Chrome does
 * "fast shutdown" of a web page.
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
 * PPP_GetInterface() is called by the browser to query the module for
 * interfaces it supports.
 *
 * Your module must implement the PPP_Instance interface or it will be
 * unloaded. Other interfaces are optional.
 *
 * @param[in] interface_name A pointer to a "PPP" (plugin) interface name.
 * Interface names are null-terminated ASCII strings.
 *
 * @return A pointer for the interface or NULL if the interface is not
 * supported.
 */
PP_EXPORT const void* PPP_GetInterface(const char* interface_name);
/**
 * @}
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* PPAPI_C_PPP_H_ */
