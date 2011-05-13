/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_MESSAGING_H_
#define PPAPI_C_PPB_MESSAGING_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"

#define PPB_MESSAGING_INTERFACE_0_1 "PPB_Messaging;0.1"
#define PPB_MESSAGING_INTERFACE PPB_MESSAGING_INTERFACE_0_1

/**
 * @file
 * This file defines the PPB_Messaging interface implemented by the browser.
 * The PPB_Messaging interface contains pointers to functions related to
 * sending messages to JavaScript message event listeners on the DOM element
 * associated with a specific module instance.
 *
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_Messaging interface contains pointers to functions related to
 * sending messages to JavaScript message event listeners on the DOM element
 * associated with a specific module instance.
 */
struct PPB_Messaging {
  /**
   * @a PostMessage is a pointer to a function which asynchronously invokes any
   * listeners for message events on the DOM element for the given module
   * instance. This means that a call to @a PostMessage will not block while the
   * message is processed.
   *
   * @param message is a PP_Var containing the data to be sent to JavaScript.
   * Currently, it can have an int32_t, double, bool, or string value (objects
   * are not supported.)
   *
   * Listeners for message events in JavaScript code will receive an object
   * conforming to the MessageEvent interface. In particular, the value of @a
   * message will be contained as a property called @a data in the received
   * MessageEvent.
   *
   * This is analogous to listening for messages from Web Workers.
   *
   * See:
   *   http://www.whatwg.org/specs/web-workers/current-work/
   *
   * For example:
   *
   * @verbatim
   *
   * <body>
   *   <object id="plugin"
   *           type="application/x-ppapi-postMessage-example"/>
   *   <script type="text/javascript">
   *     var plugin = document.getElementById('plugin');
   *     plugin.AddEventListener("message",
   *                             function(message) { alert(message.data); },
   *                             false);
   *   </script>
   * </body>
   *
   * @endverbatim
   *
   * If the module instance then invokes @a PostMessage() as follows:
   * <code>
   *  char hello_world[] = "Hello world!";
   *  PP_Var hello_var = ppb_var_if->VarFromUtf8(instance,
   *                                             hello_world,
   *                                             sizeof(hello_world));
   *  ppb_messaging_if->PostMessage(instance, hello_var);
   * </code>
   *
   * The browser will pop-up an alert saying "Hello world!".
   */
  void (*PostMessage)(PP_Instance instance, struct PP_Var message);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MESSAGING_H_ */

