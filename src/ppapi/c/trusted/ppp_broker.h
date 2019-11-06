/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppp_broker.idl modified Sat Jul 16 16:51:03 2011. */

#ifndef PPAPI_C_TRUSTED_PPP_BROKER_H_
#define PPAPI_C_TRUSTED_PPP_BROKER_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * This file defines functions that your module must implement to support a
 * broker.
 */


// {PENDING: undefine PP_EXPORT?}

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"


#if __GNUC__ >= 4

#define PP_EXPORT __attribute__ ((visibility("default")))
#elif defined(_MSC_VER)
#define PP_EXPORT __declspec(dllexport)
#endif



/* We don't want name mangling for these external functions.  We only need
 * 'extern "C"' if we're compiling with a C++ compiler.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup Typedefs
 * @{
 */

/**
 * PP_ConnectInstance_Func defines the signature that you implement to
 * receive notifications when a plugin instance connects to the broker.
 * The broker should listen on the socket before returning.
 *
 * @param[in] instance The plugin instance connecting to the broker.
 * @param[in] handle Handle to a socket the broker can use to communicate with
 * the plugin.
 * @return PP_OK on success. Any other value on failure.
 */
typedef int32_t (*PP_ConnectInstance_Func)(PP_Instance instance,
                                           int32_t handle);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PPP_InitializeBroker() is the entry point for a broker and is
 * called by the browser when your module loads. Your code must implement this
 * function.
 *
 * Failure indicates to the browser that this broker can not be used. In this
 * case, the broker will be unloaded.
 *
 * @param[out] connect_instance_func A pointer to a connect instance function.
 * @return PP_OK on success. Any other value on failure.
*/
PP_EXPORT int32_t PPP_InitializeBroker(
    PP_ConnectInstance_Func* connect_instance_func);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/** PPP_ShutdownBroker() is called before the broker is unloaded.
 */
PP_EXPORT void PPP_ShutdownBroker();
/**
 * @}
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* PPAPI_C_TRUSTED_PPP_BROKER_H_ */

