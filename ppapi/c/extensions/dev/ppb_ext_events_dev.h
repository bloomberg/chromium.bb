/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From extensions/dev/ppb_ext_events_dev.idl,
 *   modified Mon Mar 18 17:18:20 2013.
 */

#ifndef PPAPI_C_EXTENSIONS_DEV_PPB_EXT_EVENTS_DEV_H_
#define PPAPI_C_EXTENSIONS_DEV_PPB_EXT_EVENTS_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_EXT_EVENTS_DEV_INTERFACE_0_1 "PPB_Ext_Events(Dev);0.1"
#define PPB_EXT_EVENTS_DEV_INTERFACE PPB_EXT_EVENTS_DEV_INTERFACE_0_1

/**
 * @file
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * Used to represent arbitrary C function pointers. Please note that usually
 * the function that a <code>PP_Ext_GenericFuncType</code> pointer points to
 * has a different signature than <code>void (*)()</code>.
 */
typedef void (*PP_Ext_GenericFuncType)(void);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * An event listener that can be registered with the browser and receive
 * notifications via the callback function.
 *
 * A function is defined for each event type to return a properly-filled
 * <code>PP_Ext_EventListener</code> struct, for example,
 * <code>PP_Ext_Alarms_OnAlarm_Dev()</code>.
 */
struct PP_Ext_EventListener {
  /**
   * The name of the event to register to.
   */
  const char* event_name;
  /**
   * A callback function whose signature is determined by
   * <code>event_name</code>. All calls will happen on the same thread as the
   * one on which <code>AddListener()</code> is called.
   */
  PP_Ext_GenericFuncType func;
  /**
   * An opaque pointer that will be passed to <code>func</code>.
   */
  void* user_data;
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Ext_Events_Dev_0_1 {
  /**
   * Registers a listener to an event.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   * @param[in] listener A <code>PP_Ext_EventListener</code> struct.
   *
   * @return An listener ID, or 0 if failed.
   */
  uint32_t (*AddListener)(PP_Instance instance,
                          struct PP_Ext_EventListener listener);
  /**
   * Deregisters a listener.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   * @param[in] listener_id The ID returned by <code>AddListener()</code>.
   */
  void (*RemoveListener)(PP_Instance instance, uint32_t listener_id);
};

typedef struct PPB_Ext_Events_Dev_0_1 PPB_Ext_Events_Dev;
/**
 * @}
 */

/**
 * Creates a <code>PP_Ext_EventListener</code> struct.
 *
 * Usually you should not call it directly. Instead you should call those
 * functions that return a <code>PP_Ext_EventListener</code> struct for a
 * specific event type, for example, <code>PP_Ext_Alarms_OnAlarm_Dev()</code>.
 */
PP_INLINE struct PP_Ext_EventListener PP_Ext_MakeEventListener(
    const char* event_name,
    PP_Ext_GenericFuncType func,
    void* user_data) {
  struct PP_Ext_EventListener listener;
  listener.event_name = event_name;
  listener.func = func;
  listener.user_data = user_data;
  return listener;
}
#endif  /* PPAPI_C_EXTENSIONS_DEV_PPB_EXT_EVENTS_DEV_H_ */

