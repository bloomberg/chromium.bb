/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From extensions/dev/ppb_ext_alarms_dev.idl,
 *   modified Wed Mar 20 13:50:11 2013.
 */

#ifndef PPAPI_C_EXTENSIONS_DEV_PPB_EXT_ALARMS_DEV_H_
#define PPAPI_C_EXTENSIONS_DEV_PPB_EXT_ALARMS_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_EXT_ALARMS_DEV_INTERFACE_0_1 "PPB_Ext_Alarms(Dev);0.1"
#define PPB_EXT_ALARMS_DEV_INTERFACE PPB_EXT_ALARMS_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the Pepper equivalent of the <code>chrome.alarms</code>
 * extension API.
 */


#include "ppapi/c/extensions/dev/ppb_ext_events_dev.h"

/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * A dictionary <code>PP_Var</code> which contains:
 * - "name" : string <code>PP_Var</code>
 * Name of this alarm.
 *
 * - "scheduledTime" : double <code>PP_Var</code>
 * Time at which this alarm was scheduled to fire, in milliseconds past the
 * epoch (e.g. <code>Date.now() + n</code>).  For performance reasons, the
 * alarm may have been delayed an arbitrary amount beyond this.
 *
 * - "periodInMinutes" : double or undefined <code>PP_Var</code>
 * If not undefined, the alarm is a repeating alarm and will fire again in
 * <var>periodInMinutes</var> minutes.
 */
typedef struct PP_Var PP_Ext_Alarms_Alarm_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "when" : double or undefined <code>PP_Var</code>
 * Time at which the alarm should fire, in milliseconds past the epoch
 * (e.g. <code>Date.now() + n</code>).
 *
 * - "delayInMinutes" : double or undefined <code>PP_Var</code>
 * Length of time in minutes after which the
 * <code>PP_Ext_Alarms_OnAlarm_Dev</code> event should fire.
 *
 * - "periodInMinutes" : double or undefined <code>PP_Var</code>
 * If set, the <code>PP_Ext_Alarms_OnAlarm_Dev</code> event should fire every
 * <var>periodInMinutes</var> minutes after the initial event specified by
 * <var>when</var> or <var>delayInMinutes</var>.  If not set, the alarm will
 * only fire once.
 */
typedef struct PP_Var PP_Ext_Alarms_AlarmCreateInfo_Dev;

/**
 * An array <code>PP_Var</code> which contains elements of
 * <code>PP_Ext_Alarms_Alarm_Dev</code>.
 */
typedef struct PP_Var PP_Ext_Alarms_Alarm_Dev_Array;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Ext_Alarms_Dev_0_1 {
  /**
   * Creates an alarm.  Near the time(s) specified by <var>alarm_info</var>,
   * the <code>PP_Ext_Alarms_OnAlarm_Dev</code> event is fired. If there is
   * another alarm with the same name (or no name if none is specified), it will
   * be cancelled and replaced by this alarm.
   *
   * In order to reduce the load on the user's machine, Chrome limits alarms
   * to at most once every 1 minute but may delay them an arbitrary amount
   * more.  That is, setting
   * <code>$ref:[PP_Ext_Alarms_AlarmCreateInfo_Dev.delayInMinutes
   * delayInMinutes]</code> or
   * <code>$ref:[PP_Ext_Alarms_AlarmCreateInfo_Dev.periodInMinutes
   * periodInMinutes]</code> to less than <code>1</code> will not be honored
   * and will cause a warning.
   * <code>$ref:[PP_Ext_Alarms_AlarmCreateInfo_Dev.when when]</code> can be set
   * to less than 1 minute after "now" without warning but won't actually cause
   * the alarm to fire for at least 1 minute.
   *
   * To help you debug your app or extension, when you've loaded it unpacked,
   * there's no limit to how often the alarm can fire.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] name A string or undefined <code>PP_Var</code>. Optional name to
   * identify this alarm. Defaults to the empty string.
   * @param[in] alarm_info A <code>PP_Var</code> whose contents conform to the
   * description of <code>PP_Ext_Alarms_AlarmCreateInfo_Dev</code>. Describes
   * when the alarm should fire.  The initial time must be specified by either
   * <var>when</var> or <var>delayInMinutes</var> (but not both).  If
   * <var>periodInMinutes</var> is set, the alarm will repeat every
   * <var>periodInMinutes</var> minutes after the initial event.  If neither
   * <var>when</var> or <var>delayInMinutes</var> is set for a repeating alarm,
   * <var>periodInMinutes</var> is used as the default for
   * <var>delayInMinutes</var>.
   */
  void (*Create)(PP_Instance instance,
                 struct PP_Var name,
                 PP_Ext_Alarms_AlarmCreateInfo_Dev alarm_info);
  /**
   * Retrieves details about the specified alarm.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] name A string or undefined <code>PP_Var</code>. The name of the
   * alarm to get. Defaults to the empty string.
   * @param[out] alarm A <code>PP_Var</code> whose contents conform to the
   * description of <code>PP_Ext_Alarms_Alarm_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>
   */
  int32_t (*Get)(PP_Instance instance,
                 struct PP_Var name,
                 PP_Ext_Alarms_Alarm_Dev* alarm,
                 struct PP_CompletionCallback callback);
  /**
   * Gets an array of all the alarms.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[out] alarms A <code>PP_Var</code> whose contents conform to the
   * description of <code>PP_Ext_Alarms_Alarm_Dev_Array</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>
   */
  int32_t (*GetAll)(PP_Instance instance,
                    PP_Ext_Alarms_Alarm_Dev_Array* alarms,
                    struct PP_CompletionCallback callback);
  /**
   * Clears the alarm with the given name.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] name A string or undefined <code>PP_Var</code>. The name of the
   * alarm to clear. Defaults to the empty string.
   */
  void (*Clear)(PP_Instance instance, struct PP_Var name);
  /**
   * Clears all alarms.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   */
  void (*ClearAll)(PP_Instance instance);
};

typedef struct PPB_Ext_Alarms_Dev_0_1 PPB_Ext_Alarms_Dev;
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * Fired when an alarm has elapsed. Useful for event pages.
 *
 * @param[in] listener_id The listener ID.
 * @param[inout] user_data The opaque pointer that was used when registering the
 * listener.
 * @param[in] alarm A <code>PP_Var</code> whose contents conform to the
 * description of <code>PP_Ext_Alarms_Alarm_Dev</code>. The alarm that has
 * elapsed.
 */
typedef void (*PP_Ext_Alarms_OnAlarm_Func_Dev_0_1)(
    uint32_t listener_id,
    void* user_data,
    PP_Ext_Alarms_Alarm_Dev alarm);
/**
 * @}
 */

PP_INLINE struct PP_Ext_EventListener PP_Ext_Alarms_OnAlarm_Dev_0_1(
    PP_Ext_Alarms_OnAlarm_Func_Dev_0_1 func,
    void* user_data) {
  return PP_Ext_MakeEventListener("alarms.onAlarm;0.1",
                                  (PP_Ext_GenericFuncType)(func), user_data);
}

#define PP_Ext_Alarms_OnAlarm_Dev PP_Ext_Alarms_OnAlarm_Dev_0_1
#endif  /* PPAPI_C_EXTENSIONS_DEV_PPB_EXT_ALARMS_DEV_H_ */

