// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/extensions/dev/alarms_dev.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/var_dictionary_dev.h"
#include "ppapi/cpp/extensions/optional.h"
#include "ppapi/cpp/extensions/to_var_converter.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Ext_Alarms_Dev_0_1>() {
  return PPB_EXT_ALARMS_DEV_INTERFACE_0_1;
}

}  // namespace

namespace ext {
namespace alarms {

const char* const Alarm_Dev::kName = "name";
const char* const Alarm_Dev::kScheduledTime = "scheduledTime";
const char* const Alarm_Dev::kPeriodInMinutes = "periodInMinutes";

Alarm_Dev::Alarm_Dev()
    : name(kName),
      scheduled_time(kScheduledTime),
      period_in_minutes(kPeriodInMinutes) {
}

Alarm_Dev::~Alarm_Dev() {
}

bool Alarm_Dev::Populate(const PP_Ext_Alarms_Alarm_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = name.Populate(dict);
  result = scheduled_time.Populate(dict) && result;
  result = period_in_minutes.Populate(dict) && result;

  return result;
}

Var Alarm_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = name.AddTo(&dict);
  result = scheduled_time.AddTo(&dict) && result;
  result = period_in_minutes.MayAddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

const char* const AlarmCreateInfo_Dev::kWhen = "when";
const char* const AlarmCreateInfo_Dev::kDelayInMinutes = "delayInMinutes";
const char* const AlarmCreateInfo_Dev::kPeriodInMinutes = "periodInMinutes";

AlarmCreateInfo_Dev::AlarmCreateInfo_Dev()
    : when(kWhen),
      delay_in_minutes(kDelayInMinutes),
      period_in_minutes(kPeriodInMinutes) {
}

AlarmCreateInfo_Dev::~AlarmCreateInfo_Dev() {
}

bool AlarmCreateInfo_Dev::Populate(
    const PP_Ext_Alarms_AlarmCreateInfo_Dev& value) {
  if (value.type != PP_VARTYPE_DICTIONARY)
    return false;

  VarDictionary_Dev dict(value);
  bool result = when.Populate(dict);
  result = delay_in_minutes.Populate(dict) && result;
  result = period_in_minutes.Populate(dict) && result;

  return result;
}

Var AlarmCreateInfo_Dev::CreateVar() const {
  VarDictionary_Dev dict;

  bool result = when.MayAddTo(&dict);
  result = delay_in_minutes.MayAddTo(&dict) && result;
  result = period_in_minutes.MayAddTo(&dict) && result;
  PP_DCHECK(result);

  return dict;
}

Alarms_Dev::Alarms_Dev(const InstanceHandle& instance) : instance_(instance) {
}

Alarms_Dev::~Alarms_Dev() {
}

void Alarms_Dev::Create(const Optional<std::string>& name,
                        const AlarmCreateInfo_Dev& alarm_info) {
  if (!has_interface<PPB_Ext_Alarms_Dev_0_1>())
    return;

  internal::ToVarConverter<Optional<std::string> > name_var(name);
  internal::ToVarConverter<AlarmCreateInfo_Dev> alarm_info_var(alarm_info);

  return get_interface<PPB_Ext_Alarms_Dev_0_1>()->Create(
      instance_.pp_instance(),
      name_var.pp_var(),
      alarm_info_var.pp_var());
}

int32_t Alarms_Dev::Get(
    const Optional<std::string>& name,
    const CompletionCallbackWithOutput<Alarm_Dev>& callback) {
  if (!has_interface<PPB_Ext_Alarms_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToVarConverter<Optional<std::string> > name_var(name);

  return get_interface<PPB_Ext_Alarms_Dev_0_1>()->Get(
      instance_.pp_instance(),
      name_var.pp_var(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Alarms_Dev::GetAll(
    const CompletionCallbackWithOutput<std::vector<Alarm_Dev> >& callback) {
  if (!has_interface<PPB_Ext_Alarms_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_Ext_Alarms_Dev_0_1>()->GetAll(
      instance_.pp_instance(),
      callback.output(),
      callback.pp_completion_callback());
}

void Alarms_Dev::Clear(const Optional<std::string>& name) {
  if (!has_interface<PPB_Ext_Alarms_Dev_0_1>())
    return;

  internal::ToVarConverter<Optional<std::string> > name_var(name);

  return get_interface<PPB_Ext_Alarms_Dev_0_1>()->Clear(
      instance_.pp_instance(),
      name_var.pp_var());
}

void Alarms_Dev::ClearAll() {
  if (!has_interface<PPB_Ext_Alarms_Dev_0_1>())
    return;

  return get_interface<PPB_Ext_Alarms_Dev_0_1>()->ClearAll(
      instance_.pp_instance());
}

OnAlarmEvent_Dev::OnAlarmEvent_Dev(
    const InstanceHandle& instance,
    Listener* listener)
    : internal::EventBase1<PP_Ext_Alarms_OnAlarm_Dev, Alarm_Dev>(instance),
      listener_(listener) {
}

OnAlarmEvent_Dev::~OnAlarmEvent_Dev() {
}

void OnAlarmEvent_Dev::Callback(Alarm_Dev& alarm) {
  listener_->OnAlarm(alarm);
}

}  // namespace alarms
}  // namespace ext
}  // namespace pp
