// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/chromeos/lib/rlz_value_store_chromeos.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/prefs/json_pref_store.h"
#include "base/sequenced_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/recursive_lock.h"
#include "rlz/lib/rlz_lib.h"

namespace rlz_lib {

namespace {

// Product names.
const char kProductChrome[] = "chrome";
const char kProductOther[] = "other";

// Key names.
const char kPingTimeKey[] = "ping_time";
const char kAccessPointKey[] = "access_points";
const char kProductEventKey[] = "product_events";
const char kStatefulEventKey[] = "stateful_events";

// Brand name used when there is no supplementary brand name.
const char kNoSupplementaryBrand[] = "_";

// RLZ store filename.
const FilePath::CharType kRLZDataFileName[] = FILE_PATH_LITERAL("RLZ Data");

// RLZ store path for testing.
FilePath g_testing_rlz_store_path_;

// Returns file path of the RLZ storage.
FilePath GetRlzStorePath() {
  return g_testing_rlz_store_path_.empty() ?
      file_util::GetHomeDir().Append(kRLZDataFileName) :
      g_testing_rlz_store_path_.Append(kRLZDataFileName);
}

// Returns the dictionary key for storing access point-related prefs.
std::string GetKeyName(std::string key, AccessPoint access_point) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetAccessPointName(access_point) + "." + brand;
}

// Returns the dictionary key for storing product-related prefs.
std::string GetKeyName(std::string key, Product product) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetProductName(product) + "." + brand;
}

}  // namespace

// static
base::SequencedTaskRunner* RlzValueStoreChromeOS::io_task_runner_ = NULL;

// static
bool RlzValueStoreChromeOS::created_;

// static
RlzValueStoreChromeOS* RlzValueStoreChromeOS::GetInstance() {
  return Singleton<RlzValueStoreChromeOS>::get();
}

// static
void RlzValueStoreChromeOS::SetIOTaskRunner(
    base::SequencedTaskRunner* io_task_runner) {
  io_task_runner_ = io_task_runner;
  // Make sure |io_task_runner_| lives until constructor is called.
  io_task_runner_->AddRef();
}

// static
void RlzValueStoreChromeOS::ResetForTesting() {
  // Make sure we don't create an instance if it didn't exist.
  if (created_)
    GetInstance()->ReadPrefs();
}

// static
void RlzValueStoreChromeOS::Cleanup() {
  if (created_)
    GetInstance()->rlz_store_ = NULL;
}

RlzValueStoreChromeOS::RlzValueStoreChromeOS() {
  ReadPrefs();
  created_ = true;
}

RlzValueStoreChromeOS::~RlzValueStoreChromeOS() {
}

bool RlzValueStoreChromeOS::HasAccess(AccessType type) {
  return type == kReadAccess || !rlz_store_->ReadOnly();
}

bool RlzValueStoreChromeOS::WritePingTime(Product product, int64 time) {
  std::string value = base::Int64ToString(time);
  rlz_store_->SetValue(GetKeyName(kPingTimeKey, product),
                       base::Value::CreateStringValue(value));
  return true;
}

bool RlzValueStoreChromeOS::ReadPingTime(Product product, int64* time) {
  const base::Value* value = NULL;
  rlz_store_->GetValue(GetKeyName(kPingTimeKey, product), &value);
  std::string s_value;
  return value && value->GetAsString(&s_value) &&
      base::StringToInt64(s_value, time);
}

bool RlzValueStoreChromeOS::ClearPingTime(Product product) {
  rlz_store_->RemoveValue(GetKeyName(kPingTimeKey, product));
  return true;
}

bool RlzValueStoreChromeOS::WriteAccessPointRlz(AccessPoint access_point,
                                                const char* new_rlz) {
  rlz_store_->SetValue(
      GetKeyName(kAccessPointKey, access_point),
      base::Value::CreateStringValue(new_rlz));
  return true;
}

bool RlzValueStoreChromeOS::ReadAccessPointRlz(AccessPoint access_point,
                                               char* rlz,
                                               size_t rlz_size) {
  const base::Value* value = NULL;
  rlz_store_->GetValue(
      GetKeyName(kAccessPointKey, access_point), &value);
  std::string s_value;
  if (value)
    value->GetAsString(&s_value);
  if (s_value.size() < rlz_size) {
    strncpy(rlz, s_value.c_str(), rlz_size);
    return true;
  }
  if (rlz_size > 0)
    *rlz = '\0';
  return false;
}

bool RlzValueStoreChromeOS::ClearAccessPointRlz(AccessPoint access_point) {
  rlz_store_->RemoveValue(
      GetKeyName(kAccessPointKey, access_point));
  return true;
}

bool RlzValueStoreChromeOS::AddProductEvent(Product product,
                                            const char* event_rlz) {
  return AddValueToList(GetKeyName(kProductEventKey, product),
                        base::Value::CreateStringValue(event_rlz));
}

bool RlzValueStoreChromeOS::ReadProductEvents(
    Product product,
    std::vector<std::string>* events) {
  base::ListValue* events_list = GetList(GetKeyName(kProductEventKey, product));
  if (!events_list)
    return false;
  events->clear();
  for (size_t i = 0; i < events_list->GetSize(); ++i) {
    std::string event;
    if (events_list->GetString(i, &event))
      events->push_back(event);
  }
  return true;
}

bool RlzValueStoreChromeOS::ClearProductEvent(Product product,
                                              const char* event_rlz) {
  base::StringValue event_value(event_rlz);
  return RemoveValueFromList(GetKeyName(kProductEventKey, product),
                             event_value);
}

bool RlzValueStoreChromeOS::ClearAllProductEvents(Product product) {
  rlz_store_->RemoveValue(GetKeyName(kProductEventKey, product));
  return true;
}

bool RlzValueStoreChromeOS::AddStatefulEvent(Product product,
                                             const char* event_rlz) {
  return AddValueToList(GetKeyName(kStatefulEventKey, product),
                        base::Value::CreateStringValue(event_rlz));
}

bool RlzValueStoreChromeOS::IsStatefulEvent(Product product,
                                            const char* event_rlz) {
  base::ListValue* events_list =
      GetList(GetKeyName(kStatefulEventKey, product));
  base::StringValue event_value(event_rlz);
  return events_list && events_list->Find(event_value) != events_list->end();
}

bool RlzValueStoreChromeOS::ClearAllStatefulEvents(Product product) {
  rlz_store_->RemoveValue(GetKeyName(kStatefulEventKey, product));
  return true;
}

void RlzValueStoreChromeOS::CollectGarbage() {
  NOTIMPLEMENTED();
}

void RlzValueStoreChromeOS::ReadPrefs() {
  DCHECK(io_task_runner_)
      << "Calling GetInstance or ResetForTesting before SetIOTaskRunner?";
  rlz_store_ = new JsonPrefStore(GetRlzStorePath(), io_task_runner_);
  rlz_store_->ReadPrefs();
  switch (rlz_store_->GetReadError()) {
    case PersistentPrefStore::PREF_READ_ERROR_NONE:
    case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
      break;
    default:
      LOG(ERROR) << "Error read RLZ store: " << rlz_store_->GetReadError();
  }
  // Restore refcount modified by SetIOTaskRunner().
  io_task_runner_->Release();
  io_task_runner_ = NULL;
}

base::ListValue* RlzValueStoreChromeOS::GetList(std::string list_name) {
  base::Value* list_value = NULL;
  rlz_store_->GetMutableValue(list_name, &list_value);
  base::ListValue* list = NULL;
  if (!list_value || !list_value->GetAsList(&list))
    return NULL;
  return list;
}

bool RlzValueStoreChromeOS::AddValueToList(std::string list_name,
                                           base::Value* value) {
  base::ListValue* list = GetList(list_name);
  if (!list) {
    list = new base::ListValue;
    rlz_store_->SetValue(list_name, list);
  }
  if (list->AppendIfNotPresent(value))
    rlz_store_->ReportValueChanged(list_name);
  return true;
}

bool RlzValueStoreChromeOS::RemoveValueFromList(std::string list_name,
                                                const base::Value& value) {
  base::ListValue* list = GetList(list_name);
  if (!list)
    return false;
  rlz_store_->MarkNeedsEmptyValue(list_name);
  size_t index;
  if (list->Remove(value, &index))
    rlz_store_->ReportValueChanged(list_name);
  return true;
}


ScopedRlzValueStoreLock::ScopedRlzValueStoreLock()
    : store_(RlzValueStoreChromeOS::GetInstance()) {
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_;
}

namespace testing {

void SetRlzStoreDirectory(const FilePath& directory) {
  g_testing_rlz_store_path_ = directory;
}

}  // namespace testing

}  // namespace rlz_lib
