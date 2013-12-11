/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_alarms_dev.idl modified Tue Dec 10 17:40:20 2013. */

#ifndef PPAPI_C_DEV_PPB_ALARMS_DEV_H_
#define PPAPI_C_DEV_PPB_ALARMS_DEV_H_

#include "ppapi/c/dev/pp_optional_structs_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_ALARMS_DEV_INTERFACE_0_1 "PPB_Alarms(Dev);0.1"
#define PPB_ALARMS_DEV_INTERFACE PPB_ALARMS_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the Pepper equivalent of the <code>chrome.alarms</code>
 * extension API.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_Alarms_Alarm_Dev {
  /**
   * Name of this alarm.
   */
  struct PP_Var name;
  /**
   * Time at which this alarm was scheduled to fire, in milliseconds past the
   * epoch. For performance reasons, the alarm may have been delayed an
   * arbitrary amount beyond this.
   */
  double scheduled_time;
  /**
   * If set, the alarm is a repeating alarm and will fire again in
   * <code>period_in_minutes</code> minutes.
   */
  struct PP_Optional_Double_Dev period_in_minutes;
};

struct PP_Alarms_AlarmCreateInfo_Dev {
  /**
   * Time at which the alarm should fire, in milliseconds past the epoch.
   */
  struct PP_Optional_Double_Dev when;
  /**
   * Length of time in minutes after which the
   * <code>PP_Alarms_OnAlarm_Dev</code> event should fire.
   */
  struct PP_Optional_Double_Dev delay_in_minutes;
  /**
   * If set, the <code>PP_Alarms_OnAlarm_Dev</code> event should fire every
   * <code>period_in_minutes</code> minutes after the initial event specified by
   * <code>when</code> or <code>delay_in_minutes</code>. If not set, the alarm
   * will only fire once.
   */
  struct PP_Optional_Double_Dev period_in_minutes;
};

struct PP_Alarms_Alarm_Array_Dev {
  uint32_t size;
  struct PP_Alarms_Alarm_Dev *elements;
};
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
 * @param[in] alarm The alarm that has elapsed.
 */
typedef void (*PP_Alarms_OnAlarm_Dev)(
    uint32_t listener_id,
    void* user_data,
    const struct PP_Alarms_Alarm_Dev* alarm);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Alarms_Dev_0_1 {
  /**
   * Creates an alarm.  Near the time(s) specified by <code>alarm_info</code>,
   * the <code>PP_Alarms_OnAlarm_Dev</code> event is fired. If there is another
   * alarm with the same name (or no name if none is specified), it will be
   * cancelled and replaced by this alarm.
   *
   * In order to reduce the load on the user's machine, Chrome limits alarms
   * to at most once every 1 minute but may delay them an arbitrary amount more.
   * That is, setting
   * <code>PP_Alarms_AlarmCreateInfo_Dev.delay_in_minutes</code> or
   * <code>PP_Alarms_AlarmCreateInfo_Dev.period_in_minutes</code> to less than
   * <code>1</code> will not be honored and will cause a warning.
   * <code>PP_Alarms_AlarmCreateInfo_Dev.when</code> can be set to less than 1
   * minute after "now" without warning but won't actually cause the alarm to
   * fire for at least 1 minute.
   *
   * To help you debug your app or extension, when you've loaded it unpacked,
   * there's no limit to how often the alarm can fire.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] name A string or undefined <code>PP_Var</code>. Optional name to
   * identify this alarm. Defaults to the empty string.
   * @param[in] alarm_info Describes when the alarm should fire. The initial
   * time must be specified by either <code>when</code> or
   * <code>delay_in_minutes</code> (but not both).  If
   * <code>period_in_minutes</code> is set, the alarm will repeat every
   * <code>period_in_minutes</code> minutes after the initial event.  If neither
   * <code>when</code> or <code>delay_in_minutes</code> is set for a repeating
   * alarm, <code>period_in_minutes</code> is used as the default for
   * <code>delay_in_minutes</code>.
   */
  void (*Create)(PP_Instance instance,
                 struct PP_Var name,
                 const struct PP_Alarms_AlarmCreateInfo_Dev* alarm_info);
  /**
   * Retrieves details about the specified alarm.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] name A string or undefined <code>PP_Var</code>. The name of the
   * alarm to get. Defaults to the empty string.
   * @param[out] alarm A <code>PP_Alarms_Alarm_Dev</code> struct to store the
   * output result.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>
   */
  int32_t (*Get)(PP_Instance instance,
                 struct PP_Var name,
                 struct PP_Alarms_Alarm_Dev* alarm,
                 struct PP_CompletionCallback callback);
  /**
   * Gets an array of all the alarms.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[out] alarms A <code>PP_Alarms_Alarm_Array_Dev</code> to store the
   * output result.
   * @param[in] array_allocator A <code>PP_ArrayOutput</code> to allocate memory
   * for <code>alarms</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>
   */
  int32_t (*GetAll)(PP_Instance instance,
                    struct PP_Alarms_Alarm_Array_Dev* alarms,
                    struct PP_ArrayOutput array_allocator,
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
  /**
   * Registers <code>PP_Alarms_OnAlarm_Dev</code> event.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] callback The callback to receive notifications.
   * @param[inout] user_data An opaque pointer that will be passed to
   * <code>callback</code>.
   *
   * @return A listener ID, or 0 if failed.
   *
   * TODO(yzshen): add a PPB_Events_Dev interface for unregistering:
   * void UnregisterListener(PP_instance instance, uint32_t listener_id);
   */
  uint32_t (*AddOnAlarmListener)(PP_Instance instance,
                                 PP_Alarms_OnAlarm_Dev callback,
                                 void* user_data);
};

typedef struct PPB_Alarms_Dev_0_1 PPB_Alarms_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_ALARMS_DEV_H_ */

