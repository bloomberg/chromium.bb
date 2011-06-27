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
 * This file defines the PPB_Messaging interface implemented by the browser
 * related to sending messages to DOM elements associated with a specific
 * module instance.
 *
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_Messaging interface contains a pointer to a function related to
 * sending messages to JavaScript message event listeners on the DOM element
 * associated with a specific module instance.
 */
struct PPB_Messaging {
  /**
   * PostMessage is a pointer to a function that asynchronously invokes any
   * listeners for message events on the DOM element for the given module
   * instance. A call to PostMessage() will not block while the message is
   * processed.
   *
   * @param[in] instance A PP_Instance indentifying one instance of a module.
   * @param[in] message A PP_Var containing the data to be sent to JavaScript.
   * Message can have a numeric, boolean, or string value; arrays and
   * dictionaries are not yet supported. Ref-counted var types are copied, and
   * are therefore not shared between the module instance and the browser.
   *
   * Listeners for message events in JavaScript code will receive an object
   * conforming to the HTML 5 MessageEvent interface. Specifically, the value of
   * message will be contained as a property called data in the received
   * MessageEvent.
   *
   * This messaging system is similar to the system used for listening for
   * messages from Web Workers. Refer to
   * http://www.whatwg.org/specs/web-workers/current-work/ for further
   * information.
   *
   * <strong>Example:</strong>
   *
   * @code
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
   * @endcode
   *
   * The module instance then invokes PostMessage() as follows:
   *
   * @code
   *
   *
   *  char hello_world[] = "Hello world!";
   *  PP_Var hello_var = ppb_var_interface->VarFromUtf8(instance,
   *                                                    hello_world,
   *                                                    sizeof(hello_world));
   *  ppb_messaging_interface->PostMessage(instance, hello_var); // Copies var.
   *  ppb_var_interface->Release(hello_var);
   *
   * @endcode
   *
   * The browser will pop-up an alert saying "Hello world!"
   */
  void (*PostMessage)(PP_Instance instance, struct PP_Var message);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MESSAGING_H_ */
