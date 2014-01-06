// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_ALARMS_DEV_H_
#define PPAPI_CPP_DEV_ALARMS_DEV_H_

#include <string>

#include "ppapi/c/dev/ppb_alarms_dev.h"
#include "ppapi/cpp/dev/may_own_ptr_dev.h"
#include "ppapi/cpp/dev/optional_dev.h"
#include "ppapi/cpp/dev/string_wrapper_dev.h"
#include "ppapi/cpp/dev/struct_wrapper_output_traits_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/output_traits.h"

namespace pp {

template <typename T>
class CompletionCallbackWithOutput;

template <typename T>
class Array;

namespace alarms {

// Data types ------------------------------------------------------------------
class Alarm_Dev {
 public:
  typedef PP_Alarms_Alarm_Dev CType;
  typedef PP_Alarms_Alarm_Array_Dev CArrayType;

  Alarm_Dev();

  Alarm_Dev(const Alarm_Dev& other);

  explicit Alarm_Dev(const PP_Alarms_Alarm_Dev& other);

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object. The contents pointed to by
  // |storage| is managed by this object.
  // Used by Pepper internal implementation.
  Alarm_Dev(PP_Alarms_Alarm_Dev* storage, NotOwned);

  ~Alarm_Dev();

  Alarm_Dev& operator=(const Alarm_Dev& other);
  Alarm_Dev& operator=(const PP_Alarms_Alarm_Dev& other);

  std::string name() const;
  void set_name(const std::string& value);

  double scheduled_time() const;
  void set_scheduled_time(double value);

  bool is_period_in_minutes_set() const;
  void unset_period_in_minutes();
  double period_in_minutes() const;
  void set_period_in_minutes(double value);

  const PP_Alarms_Alarm_Dev* ToStruct() const;

  // The returned pointer is still owned by this object. And after it is used,
  // EndRawUpdate() must be called.
  PP_Alarms_Alarm_Dev* StartRawUpdate();
  void EndRawUpdate();

 private:
  internal::MayOwnPtr<PP_Alarms_Alarm_Dev> storage_;

  internal::StringWrapper name_wrapper_;
  Optional<double> period_in_minutes_wrapper_;
};

class AlarmCreateInfo_Dev {
 public:
  typedef PP_Alarms_AlarmCreateInfo_Dev CType;

  AlarmCreateInfo_Dev();

  AlarmCreateInfo_Dev(const AlarmCreateInfo_Dev& other);

  explicit AlarmCreateInfo_Dev(const PP_Alarms_AlarmCreateInfo_Dev& other);

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object. The contents pointed to by
  // |storage| is managed by this object.
  // Used by Pepper internal implementation.
  AlarmCreateInfo_Dev(PP_Alarms_AlarmCreateInfo_Dev* storage, NotOwned);

  ~AlarmCreateInfo_Dev();

  AlarmCreateInfo_Dev& operator=(const AlarmCreateInfo_Dev& other);
  AlarmCreateInfo_Dev& operator=(const PP_Alarms_AlarmCreateInfo_Dev& other);

  bool is_when_set() const;
  void unset_when();
  double when() const;
  void set_when(double value);

  bool is_delay_in_minutes_set() const;
  void unset_delay_in_minutes();
  double delay_in_minutes() const;
  void set_delay_in_minutes(double value);

  bool is_period_in_minutes_set() const;
  void unset_period_in_minutes();
  double period_in_minutes() const;
  void set_period_in_minutes(double value);

  const PP_Alarms_AlarmCreateInfo_Dev* ToStruct() const;

  // The returned pointer is still owned by this object. And after it is used,
  // EndRawUpdate() must be called.
  PP_Alarms_AlarmCreateInfo_Dev* StartRawUpdate();
  void EndRawUpdate();

 private:
  internal::MayOwnPtr<PP_Alarms_AlarmCreateInfo_Dev> storage_;

  Optional<double> when_wrapper_;
  Optional<double> delay_in_minutes_wrapper_;
  Optional<double> period_in_minutes_wrapper_;
};

// Functions -------------------------------------------------------------------
class Alarms_Dev {
 public:
  explicit Alarms_Dev(const InstanceHandle& instance);
  ~Alarms_Dev();

  void Create(const Optional<std::string>& name,
              const AlarmCreateInfo_Dev& alarm_info);

  typedef CompletionCallbackWithOutput<Alarm_Dev> GetCallback;
  int32_t Get(const Optional<std::string>& name, const GetCallback& callback);

  typedef CompletionCallbackWithOutput<Array<Alarm_Dev> > GetAllCallback;
  int32_t GetAll(const GetAllCallback& callback);

  void Clear(const Optional<std::string>& name);

  void ClearAll();

 private:
  InstanceHandle instance_;
};

// Events ----------------------------------------------------------------------
// TODO(yzshen): add onAlarm event.

}  // namespace alarms

namespace internal {

template <>
struct CallbackOutputTraits<alarms::Alarm_Dev>
    : public internal::StructWrapperOutputTraits<alarms::Alarm_Dev> {
};

template <>
struct CallbackOutputTraits<alarms::AlarmCreateInfo_Dev>
    : public internal::StructWrapperOutputTraits<alarms::AlarmCreateInfo_Dev> {
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_ALARMS_DEV_H_
