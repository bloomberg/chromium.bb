/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppp_messaging.idl modified Mon Aug 29 10:11:34 2011. */

#ifndef PPAPI_C_PPP_MESSAGING_H_
#define PPAPI_C_PPP_MESSAGING_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPP_MESSAGING_INTERFACE_1_0 "PPP_Messaging;1.0"
#define PPP_MESSAGING_INTERFACE PPP_MESSAGING_INTERFACE_1_0

/**
 * @file
 * This file defines the PPP_Messaging interface containing pointers to
 * functions that you must implement to handle postMessage messages
 * on the associated DOM element.
 *
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_Messaging</code> interface contains pointers to functions
 * that you must implement to handle postMessage events on the associated
 * DOM element.
 */
struct PPP_Messaging {
  /**
   * HandleMessage() is a function that the browser calls when PostMessage()
   * is invoked on the DOM element for the module instance in JavaScript. Note
   * that PostMessage() in the JavaScript interface is asynchronous, meaning
   * JavaScript execution will not be blocked while HandleMessage() is
   * processing the message.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] message A <code>PP_Var</code> containing the data to be sent
   * to JavaScript. Message can have an int32_t, double, bool, or string value
   * (objects are not supported).
   *
   * The following JavaScript code invokes <code>HandleMessage</code>, passing
   * the module instance on which it was invoked, with <code>message</code>
   * being a string <code>PP_Var</code> containing "Hello world!"
   *
   * <strong>Example:</strong>
   *
   * <code>
   *
   * <body>
   *   <object id="plugin"
   *           type="application/x-ppapi-postMessage-example"/>
   *   <script type="text/javascript">
   *     document.getElementById('plugin').postMessage("Hello world!");
   *   </script>
   * </body>
   *
   * </code>
   *
   */
  void (*HandleMessage)(PP_Instance instance, struct PP_Var message);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPP_MESSAGING_H_ */

