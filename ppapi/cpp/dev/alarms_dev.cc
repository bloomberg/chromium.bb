// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/alarms_dev.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/array_dev.h"
#include "ppapi/cpp/dev/to_c_type_converter_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Alarms_Dev_0_1>() {
  return PPB_ALARMS_DEV_INTERFACE_0_1;
}

}  // namespace

namespace alarms {

Alarm_Dev::Alarm_Dev()
    : name_wrapper_(&storage_->name, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
}

Alarm_Dev::Alarm_Dev(const Alarm_Dev& other)
    : name_wrapper_(&storage_->name, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
  operator=(other);
}

Alarm_Dev::Alarm_Dev(const PP_Alarms_Alarm_Dev& other)
    : name_wrapper_(&storage_->name, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
  operator=(other);
}

Alarm_Dev::Alarm_Dev(PP_Alarms_Alarm_Dev* storage, NotOwned)
    : storage_(storage, NOT_OWNED),
      name_wrapper_(&storage_->name, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
}

Alarm_Dev::~Alarm_Dev() {
}

Alarm_Dev& Alarm_Dev::operator=(const Alarm_Dev& other) {
  return operator=(*other.storage_);
}

Alarm_Dev& Alarm_Dev::operator=(const PP_Alarms_Alarm_Dev& other) {
  if (storage_.get() == &other)
    return *this;

  name_wrapper_ = other.name;
  storage_->scheduled_time = other.scheduled_time;
  period_in_minutes_wrapper_ = other.period_in_minutes;

  return *this;
}

std::string Alarm_Dev::name() const {
  return name_wrapper_.get();
}

void Alarm_Dev::set_name(const std::string& value) {
  name_wrapper_.set(value);
}

double Alarm_Dev::scheduled_time() const {
  return storage_->scheduled_time;
}

void Alarm_Dev::set_scheduled_time(double value) {
  storage_->scheduled_time = value;
}

bool Alarm_Dev::is_period_in_minutes_set() const {
  return period_in_minutes_wrapper_.is_set();
}

void Alarm_Dev::unset_period_in_minutes() {
  period_in_minutes_wrapper_.unset();
}

double Alarm_Dev::period_in_minutes() const {
  return period_in_minutes_wrapper_.get();
}

void Alarm_Dev::set_period_in_minutes(double value) {
  period_in_minutes_wrapper_.set(value);
}

const PP_Alarms_Alarm_Dev* Alarm_Dev::ToStruct() const {
  return storage_.get();
}

PP_Alarms_Alarm_Dev* Alarm_Dev::StartRawUpdate() {
  name_wrapper_.StartRawUpdate();
  period_in_minutes_wrapper_.StartRawUpdate();

  return storage_.get();
}

void Alarm_Dev::EndRawUpdate() {
  name_wrapper_.EndRawUpdate();
  period_in_minutes_wrapper_.EndRawUpdate();
}

AlarmCreateInfo_Dev::AlarmCreateInfo_Dev()
    : when_wrapper_(&storage_->when, NOT_OWNED),
      delay_in_minutes_wrapper_(&storage_->delay_in_minutes, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
}

AlarmCreateInfo_Dev::AlarmCreateInfo_Dev(const AlarmCreateInfo_Dev& other)
    : when_wrapper_(&storage_->when, NOT_OWNED),
      delay_in_minutes_wrapper_(&storage_->delay_in_minutes, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
  operator=(other);
}

AlarmCreateInfo_Dev::AlarmCreateInfo_Dev(
    const PP_Alarms_AlarmCreateInfo_Dev& other)
    : when_wrapper_(&storage_->when, NOT_OWNED),
      delay_in_minutes_wrapper_(&storage_->delay_in_minutes, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
  operator=(other);
}

AlarmCreateInfo_Dev::AlarmCreateInfo_Dev(
    PP_Alarms_AlarmCreateInfo_Dev* storage,
    NotOwned)
    : storage_(storage, NOT_OWNED),
      when_wrapper_(&storage_->when, NOT_OWNED),
      delay_in_minutes_wrapper_(&storage_->delay_in_minutes, NOT_OWNED),
      period_in_minutes_wrapper_(&storage_->period_in_minutes, NOT_OWNED) {
}

AlarmCreateInfo_Dev::~AlarmCreateInfo_Dev() {
}

AlarmCreateInfo_Dev& AlarmCreateInfo_Dev::operator=(
    const AlarmCreateInfo_Dev& other) {
  return operator=(*other.storage_);
}

AlarmCreateInfo_Dev& AlarmCreateInfo_Dev::operator=(
    const PP_Alarms_AlarmCreateInfo_Dev& other) {
  if (storage_.get() == &other)
    return *this;

  when_wrapper_ = other.when;
  delay_in_minutes_wrapper_ = other.delay_in_minutes;
  period_in_minutes_wrapper_ = other.period_in_minutes;

  return *this;
}

bool AlarmCreateInfo_Dev::is_when_set() const {
  return when_wrapper_.is_set();
}

void AlarmCreateInfo_Dev::unset_when() {
  when_wrapper_.unset();
}

double AlarmCreateInfo_Dev::when() const {
  return when_wrapper_.get();
}

void AlarmCreateInfo_Dev::set_when(double value) {
  when_wrapper_.set(value);
}

bool AlarmCreateInfo_Dev::is_delay_in_minutes_set() const {
  return delay_in_minutes_wrapper_.is_set();
}

void AlarmCreateInfo_Dev::unset_delay_in_minutes() {
  delay_in_minutes_wrapper_.unset();
}

double AlarmCreateInfo_Dev::delay_in_minutes() const {
  return delay_in_minutes_wrapper_.get();
}

void AlarmCreateInfo_Dev::set_delay_in_minutes(double value) {
  delay_in_minutes_wrapper_.set(value);
}

bool AlarmCreateInfo_Dev::is_period_in_minutes_set() const {
  return period_in_minutes_wrapper_.is_set();
}

void AlarmCreateInfo_Dev::unset_period_in_minutes() {
  period_in_minutes_wrapper_.unset();
}

double AlarmCreateInfo_Dev::period_in_minutes() const {
  return period_in_minutes_wrapper_.get();
}

void AlarmCreateInfo_Dev::set_period_in_minutes(double value) {
  period_in_minutes_wrapper_.set(value);
}

const PP_Alarms_AlarmCreateInfo_Dev* AlarmCreateInfo_Dev::ToStruct() const {
  return storage_.get();
}

PP_Alarms_AlarmCreateInfo_Dev* AlarmCreateInfo_Dev::StartRawUpdate() {
  when_wrapper_.StartRawUpdate();
  delay_in_minutes_wrapper_.StartRawUpdate();
  period_in_minutes_wrapper_.StartRawUpdate();

  return storage_.get();
}

void AlarmCreateInfo_Dev::EndRawUpdate() {
  when_wrapper_.EndRawUpdate();
  delay_in_minutes_wrapper_.EndRawUpdate();
  period_in_minutes_wrapper_.EndRawUpdate();
}

Alarms_Dev::Alarms_Dev(const InstanceHandle& instance) : instance_(instance) {
}

Alarms_Dev::~Alarms_Dev() {
}

void Alarms_Dev::Create(const Optional<std::string>& name,
                        const AlarmCreateInfo_Dev& alarm_info) {
  if (!has_interface<PPB_Alarms_Dev_0_1>())
    return;

  internal::ToCTypeConverter<Optional<std::string> > name_converter(name);
  internal::ToCTypeConverter<AlarmCreateInfo_Dev> alarm_info_converter(
      alarm_info);

  return get_interface<PPB_Alarms_Dev_0_1>()->Create(
      instance_.pp_instance(),
      name_converter.ToCInput(),
      alarm_info_converter.ToCInput());
}

int32_t Alarms_Dev::Get(const Optional<std::string>& name,
                        const GetCallback& callback) {
  if (!has_interface<PPB_Alarms_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  internal::ToCTypeConverter<Optional<std::string> > name_converter(name);

  return get_interface<PPB_Alarms_Dev_0_1>()->Get(
      instance_.pp_instance(),
      name_converter.ToCInput(),
      callback.output(),
      callback.pp_completion_callback());
}

int32_t Alarms_Dev::GetAll(const GetAllCallback& callback) {
  if (!has_interface<PPB_Alarms_Dev_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_Alarms_Dev_0_1>()->GetAll(
      instance_.pp_instance(),
      callback.output(),
      internal::ArrayAllocator::Get(),
      callback.pp_completion_callback());
}

void Alarms_Dev::Clear(const Optional<std::string>& name) {
  if (!has_interface<PPB_Alarms_Dev_0_1>())
    return;

  internal::ToCTypeConverter<Optional<std::string> > name_converter(name);

  return get_interface<PPB_Alarms_Dev_0_1>()->Clear(
      instance_.pp_instance(),
      name_converter.ToCInput());
}

void Alarms_Dev::ClearAll() {
  if (!has_interface<PPB_Alarms_Dev_0_1>())
    return;

  return get_interface<PPB_Alarms_Dev_0_1>()->ClearAll(
      instance_.pp_instance());
}

}  // namespace alarms
}  // namespace pp
