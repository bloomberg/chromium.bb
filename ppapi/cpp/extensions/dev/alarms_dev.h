// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_DEV_ALARMS_DEV_H_
#define PPAPI_CPP_EXTENSIONS_DEV_ALARMS_DEV_H_

#include <string>
#include <vector>

#include "ppapi/c/extensions/dev/ppb_ext_alarms_dev.h"
#include "ppapi/cpp/extensions/dict_field.h"
#include "ppapi/cpp/extensions/event_base.h"
#include "ppapi/cpp/extensions/ext_output_traits.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"

namespace pp {

template <class T>
class CompletionCallbackWithOutput;

namespace ext {

template <class T>
class Optional;

namespace alarms {

// Data types ------------------------------------------------------------------
class Alarm_Dev : public internal::OutputObjectBase {
 public:
  Alarm_Dev();
  ~Alarm_Dev();

  bool Populate(const PP_Ext_Alarms_Alarm_Dev& value);

  Var CreateVar() const;

  static const char* const kName;
  static const char* const kScheduledTime;
  static const char* const kPeriodInMinutes;

  DictField<std::string> name;
  DictField<double> scheduled_time;
  OptionalDictField<double> period_in_minutes;
};

class AlarmCreateInfo_Dev {
 public:
  AlarmCreateInfo_Dev();
  ~AlarmCreateInfo_Dev();

  bool Populate(const PP_Ext_Alarms_AlarmCreateInfo_Dev& value);

  Var CreateVar() const;

  static const char* const kWhen;
  static const char* const kDelayInMinutes;
  static const char* const kPeriodInMinutes;

  OptionalDictField<double> when;
  OptionalDictField<double> delay_in_minutes;
  OptionalDictField<double> period_in_minutes;
};

// Functions -------------------------------------------------------------------
class Alarms_Dev {
 public:
  explicit Alarms_Dev(const InstanceHandle& instance);
  ~Alarms_Dev();

  void Create(const Optional<std::string>& name,
              const AlarmCreateInfo_Dev& alarm_info);
  int32_t Get(const Optional<std::string>& name,
              const CompletionCallbackWithOutput<Alarm_Dev>& callback);
  int32_t GetAll(
      const CompletionCallbackWithOutput<std::vector<Alarm_Dev> >& callback);
  void Clear(const Optional<std::string>& name);
  void ClearAll();

 private:
  InstanceHandle instance_;
};

// Events ----------------------------------------------------------------------
// Please see ppapi/cpp/extensions/event_base.h for how to use an event class.

class OnAlarmEvent_Dev
    : public internal::EventBase1<PP_Ext_Alarms_OnAlarm_Dev, Alarm_Dev> {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnAlarm(Alarm_Dev& alarm) = 0;
  };

  // |listener| is not owned by this instance and must outlive it.
  OnAlarmEvent_Dev(const InstanceHandle& instance, Listener* listener);
  virtual ~OnAlarmEvent_Dev();

 private:
  virtual void Callback(Alarm_Dev& alarm);

  Listener* listener_;
};

}  // namespace alarms
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_DEV_ALARMS_DEV_H_
