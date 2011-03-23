/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_MESSAGING_DEV_H_
#define PPAPI_C_DEV_PPP_MESSAGING_DEV_H_

#include "ppapi/c/pp_instance.h"

struct PP_Var;

#define PPP_MESSAGING_DEV_INTERFACE "PPP_Messaging_Dev;0.1"

/**
 * @file
 * This file defines the PPP_Messaging_Dev structure - a series of pointers to
 * methods that you must implement if you wish to handle messages posted to the
 * module instance via calls to postMessage on the associated DOM element.
 *
 */

/** @addtogroup Interfaces
 * @{
 */

/**
 * The PPP_Messaging_Dev interface contains pointers to a series of functions
 * that you must implement if you wish to handle messages posted to the module
 * instance via calls to postMessage on the associated DOM element.
 */
struct PPP_Messaging_Dev {
  /**
   * HandleMessage is a pointer to a function that the browser will call when
   * @a postMessage() is invoked on the DOM element for the module instance in
   * JavaScript. Note that @a postMessage() in the JavaScript interface is
   * asynchronous, meaning JavaScript execution will not be blocked while
   * @a HandleMessage() is processing the given @a message.
   *
   * For example:
   *
   * @verbatim
   *
   * <body>
   *   <object id="plugin"
   *           type="application/x-ppapi-postMessage-example"/>
   *   <script type="text/javascript">
   *     document.getElementById('plugin').postMessage("Hello world!");
   *   </script>
   * </body>
   *
   * @endverbatim
   *
   * This will result in @a HandleMessage being invoked, passing the module
   * instance on which it was invoked, with @a message being a string PP_Var
   * containing "Hello world!".
   */
  void (*HandleMessage)(PP_Instance instance, struct PP_Var message);
};
/**
 * @}
 */
#endif  /* PPAPI_C_DEV_PPP_MESSAGING_DEV_H_ */

