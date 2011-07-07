/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_PPP_INPUT_EVENT_H_
#define PPAPI_C_PPP_INPUT_EVENT_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

#define PPP_INPUT_EVENT_INTERFACE_0_1 "PPP_InputEvent;0.1"
#define PPP_INPUT_EVENT_INTERFACE PPP_INPUT_EVENT_INTERFACE_0_1

struct PPP_InputEvent {
  /**
   * Function for receiving input events from the browser.
   *
   * In order to receive input events, you must register for them by calling
   * PPB_InputEvent.RequestInputEvents() or RequestFilteringInputEvents(). By
   * default, no events are delivered.
   *
   * In general, you should try to keep input event handling short. Especially
   * for filtered input events, the browser or page may be blocked waiting for
   * you to respond.
   *
   * The caller of this function will maintain a reference to the input event
   * resource during this call. Unless you take a reference to the resource
   * to hold it for later, you don't need to release it.
   *
   * @return PP_TRUE if the event was handled, PP_FALSE if not. If you have
   * registered to filter this class of events by calling
   * RequestFilteringInputEvents, and you return PP_FALSE, the event will
   * be forwarded to the page (and eventually the browser) for the default
   * handling. For non-filtered events, the return value will be ignored.
   */
  PP_Bool (*HandleInputEvent)(PP_Instance instance, PP_Resource input_event);
};

#endif  // PPAPI_C_PPP_INPUT_EVENT_H_
