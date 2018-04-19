// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/chromeos/lib/rlz_value_store_chromeos.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/system/statistics_provider.h"
#include "rlz/lib/financial_ping.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/recursive_cross_process_lock_posix.h"
#include "rlz/lib/rlz_lib.h"

namespace rlz_lib {

namespace {

// Key names.
const char kPingTimeKey[] = "ping_time";
const char kAccessPointKey[] = "access_points";
const char kProductEventKey[] = "product_events";
const char kStatefulEventKey[] = "stateful_events";

// Brand name used when there is no supplementary brand name.
const char kNoSupplementaryBrand[] = "_";

// RLZ store filename.
const base::FilePath::CharType kRLZDataFileName[] =
    FILE_PATH_LITERAL("RLZ Data");

// RLZ store lock filename
const base::FilePath::CharType kRLZLockFileName[] =
    FILE_PATH_LITERAL("RLZ Data.lock");

// RLZ store path for testing.
base::LazyInstance<base::FilePath>::Leaky g_testing_rlz_store_path =
    LAZY_INSTANCE_INITIALIZER;

base::FilePath GetRlzStorePathCommon() {
  base::FilePath homedir;
  PathService::Get(base::DIR_HOME, &homedir);
  return g_testing_rlz_store_path.Get().empty()
             ? homedir
             : g_testing_rlz_store_path.Get();
}

// Returns file path of the RLZ storage.
base::FilePath GetRlzStorePath() {
  return GetRlzStorePathCommon().Append(kRLZDataFileName);
}

// Returns file path of the RLZ storage lock file.
base::FilePath GetRlzStoreLockPath() {
  return GetRlzStorePathCommon().Append(kRLZLockFileName);
}

// Returns the dictionary key for storing access point-related prefs.
std::string GetKeyName(const std::string& key, AccessPoint access_point) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetAccessPointName(access_point) + "." + brand;
}

// Returns the dictionary key for storing product-related prefs.
std::string GetKeyName(const std::string& key, Product product) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetProductName(product) + "." + brand;
}

// Returns true if the |rlz_embargo_end_date| present in VPD has passed
// compared to the current time.
bool HasRlzEmbargoEndDatePassed() {
  chromeos::system::StatisticsProvider* stats =
      chromeos::system::StatisticsProvider::GetInstance();

  std::string rlz_embargo_end_date;
  if (!stats->GetMachineStatistic(chromeos::system::kRlzEmbargoEndDateKey,
                                  &rlz_embargo_end_date)) {
    // |rlz_embargo_end_date| only exists on new devices that have not yet
    // launched. When the field doesn't exist, returns true so it's a no-op.
    return true;
  }
  base::Time parsed_time;
  if (!base::Time::FromUTCString(rlz_embargo_end_date.c_str(), &parsed_time)) {
    LOG(ERROR) << "|rlz_embargo_end_date| exists but cannot be parsed.";
    return true;
  }

  if (parsed_time - base::Time::Now() >=
      base::TimeDelta::FromDays(
          RlzValueStoreChromeOS::kRlzEmbargoEndDateGarbageDateThresholdDays)) {
    // If |rlz_embargo_end_date| is more than this many days in the future,
    // ignore it. Because it indicates that the device is not connected to an
    // ntp server in the factory, and its internal clock could be off when the
    // date is written.
    return true;
  }

  return base::Time::Now() > parsed_time;
}

}  // namespace

const int RlzValueStoreChromeOS::kRlzEmbargoEndDateGarbageDateThresholdDays =
    14;

const int RlzValueStoreChromeOS::kMaxRetryCount = 3;

RlzValueStoreChromeOS::RlzValueStoreChromeOS(const base::FilePath& store_path)
    : rlz_store_(new base::DictionaryValue),
      store_path_(store_path),
      read_only_(true),
      weak_ptr_factory_(this) {
  ReadStore();
}

RlzValueStoreChromeOS::~RlzValueStoreChromeOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WriteStore();
}

bool RlzValueStoreChromeOS::HasAccess(AccessType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type == kReadAccess || !read_only_;
}

bool RlzValueStoreChromeOS::WritePingTime(Product product, int64_t time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->SetString(GetKeyName(kPingTimeKey, product),
                        base::Int64ToString(time));
  return true;
}

bool RlzValueStoreChromeOS::ReadPingTime(Product product, int64_t* time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(wzang): make sure time is correct (check that npupdate has updated
  // successfully).
  if (!HasRlzEmbargoEndDatePassed()) {
    *time = FinancialPing::GetSystemTimeAsInt64();
    return true;
  }

  std::string ping_time;
  return rlz_store_->GetString(GetKeyName(kPingTimeKey, product), &ping_time) &&
      base::StringToInt64(ping_time, time);
}

bool RlzValueStoreChromeOS::ClearPingTime(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->Remove(GetKeyName(kPingTimeKey, product), NULL);
  return true;
}

bool RlzValueStoreChromeOS::WriteAccessPointRlz(AccessPoint access_point,
                                                const char* new_rlz) {
  // If an access point already exists, don't overwrite it.  This is to prevent
  // writing cohort data for first search which is not needed in Chrome OS.
  //
  // There are two possible cases: either the user performs a search before the
  // first ping is sent on first run, or they do not.  If they do, then
  // |new_rlz| contain cohorts for install and first search, but they will be
  // the same.  If they don't, the first time WriteAccessPointRlz() is called
  // |new_rlz| will contain only install cohort.  The second time it will
  // contain both install and first search cohorts.  Ignoring the second
  // means the first search cohort will never be stored.
  char dummy[kMaxRlzLength + 1];
  if (ReadAccessPointRlz(access_point, dummy, arraysize(dummy)) &&
      dummy[0] != 0) {
    return true;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->SetString(
      GetKeyName(kAccessPointKey, access_point), new_rlz);
  return true;
}

bool RlzValueStoreChromeOS::ReadAccessPointRlz(AccessPoint access_point,
                                               char* rlz,
                                               size_t rlz_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string rlz_value;
  rlz_store_->GetString(GetKeyName(kAccessPointKey, access_point), &rlz_value);
  if (rlz_value.size() < rlz_size) {
    strncpy(rlz, rlz_value.c_str(), rlz_size);
    return true;
  }
  if (rlz_size > 0)
    *rlz = '\0';
  return false;
}

bool RlzValueStoreChromeOS::ClearAccessPointRlz(AccessPoint access_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->Remove(GetKeyName(kAccessPointKey, access_point), NULL);
  return true;
}

bool RlzValueStoreChromeOS::AddProductEvent(Product product,
                                            const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddValueToList(GetKeyName(kProductEventKey, product),
                        std::make_unique<base::Value>(event_rlz));
}

bool RlzValueStoreChromeOS::ReadProductEvents(
    Product product,
    std::vector<std::string>* events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ListValue* events_list = nullptr;
  if (!rlz_store_->GetList(GetKeyName(kProductEventKey, product), &events_list))
    return false;
  events->clear();
  for (size_t i = 0; i < events_list->GetSize(); ++i) {
    std::string event;
    if (events_list->GetString(i, &event)) {
      if (event == "CAF" && IsStatefulEvent(product, event.c_str())) {
        base::Value event_value(event);
        size_t index;
        events_list->Remove(event_value, &index);
        --i;
        continue;
      }
      events->push_back(event);
    }
  }
  return true;
}

bool RlzValueStoreChromeOS::ClearProductEvent(Product product,
                                              const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value event_value(event_rlz);
  return RemoveValueFromList(GetKeyName(kProductEventKey, product),
                             event_value);
}

bool RlzValueStoreChromeOS::ClearAllProductEvents(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->Remove(GetKeyName(kProductEventKey, product), NULL);
  return true;
}

bool RlzValueStoreChromeOS::AddStatefulEvent(Product product,
                                             const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (strcmp(event_rlz, "CAF") == 0) {
    set_rlz_ping_sent_attempts_ = 0;
    SetRlzPingSent();
  }
  return AddValueToList(GetKeyName(kStatefulEventKey, product),
                        std::make_unique<base::Value>(event_rlz));
}

bool RlzValueStoreChromeOS::IsStatefulEvent(Product product,
                                            const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value event_value(event_rlz);
  base::ListValue* events_list = NULL;
  const bool event_exists =
      rlz_store_->GetList(GetKeyName(kStatefulEventKey, product),
                          &events_list) &&
      events_list->Find(event_value) != events_list->end();

  if (strcmp(event_rlz, "CAF") == 0) {
    chromeos::system::StatisticsProvider* stats =
        chromeos::system::StatisticsProvider::GetInstance();
    std::string should_send_rlz_ping_value;
    if (stats->GetMachineStatistic(chromeos::system::kShouldSendRlzPingKey,
                                   &should_send_rlz_ping_value)) {
      if (should_send_rlz_ping_value ==
          chromeos::system::kShouldSendRlzPingValueFalse) {
        return true;
      }
      if (!HasRlzEmbargoEndDatePassed())
        return true;

      DCHECK_EQ(should_send_rlz_ping_value,
                chromeos::system::kShouldSendRlzPingValueTrue);
    } else {
      // If |kShouldSendRlzPingKey| doesn't exist in RW_VPD, treat it in the
      // same way with the case of |kShouldSendRlzPingValueFalse|.
      return true;
    }
  }

  return event_exists;
}

bool RlzValueStoreChromeOS::ClearAllStatefulEvents(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_->Remove(GetKeyName(kStatefulEventKey, product), NULL);
  return true;
}

void RlzValueStoreChromeOS::CollectGarbage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void RlzValueStoreChromeOS::ReadStore() {
  int error_code = 0;
  std::string error_msg;
  JSONFileValueDeserializer deserializer(store_path_);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  switch (error_code) {
    case JSONFileValueDeserializer::JSON_NO_SUCH_FILE:
      read_only_ = false;
      break;
    case JSONFileValueDeserializer::JSON_NO_ERROR:
      read_only_ = false;
      rlz_store_.reset(static_cast<base::DictionaryValue*>(value.release()));
      break;
    default:
      LOG(ERROR) << "Error reading RLZ store: " << error_msg;
  }
}

void RlzValueStoreChromeOS::WriteStore() {
  std::string json_data;
  JSONStringValueSerializer serializer(&json_data);
  serializer.set_pretty_print(true);
  std::unique_ptr<base::DictionaryValue> copy =
      rlz_store_->DeepCopyWithoutEmptyChildren();
  if (!serializer.Serialize(*copy.get())) {
    LOG(ERROR) << "Failed to serialize RLZ data";
    NOTREACHED();
    return;
  }
  if (!base::ImportantFileWriter::WriteFileAtomically(store_path_, json_data))
    LOG(ERROR) << "Error writing RLZ store";
}

bool RlzValueStoreChromeOS::AddValueToList(const std::string& list_name,
                                           std::unique_ptr<base::Value> value) {
  base::ListValue* list_value = NULL;
  if (!rlz_store_->GetList(list_name, &list_value)) {
    list_value =
        rlz_store_->SetList(list_name, std::make_unique<base::ListValue>());
  }
  list_value->AppendIfNotPresent(std::move(value));
  return true;
}

bool RlzValueStoreChromeOS::RemoveValueFromList(const std::string& list_name,
                                                const base::Value& value) {
  base::ListValue* list_value = NULL;
  if (!rlz_store_->GetList(list_name, &list_value))
    return false;
  size_t index;
  list_value->Remove(value, &index);
  return true;
}

void RlzValueStoreChromeOS::SetRlzPingSent() {
  ++set_rlz_ping_sent_attempts_;
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->SetRlzPingSent(
      base::BindOnce(&RlzValueStoreChromeOS::OnSetRlzPingSent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RlzValueStoreChromeOS::OnSetRlzPingSent(bool success) {
  if (success) {
    UMA_HISTOGRAM_BOOLEAN("Rlz.SetRlzPingSent", true);
  } else if (set_rlz_ping_sent_attempts_ >= kMaxRetryCount) {
    UMA_HISTOGRAM_BOOLEAN("Rlz.SetRlzPingSent", false);
    LOG(ERROR) << "Setting |should_send_rlz_ping| to 0 failed after "
               << kMaxRetryCount << " attempts";
  } else {
    SetRlzPingSent();
  }
}

namespace {

// RlzValueStoreChromeOS keeps its data in memory and only writes it to disk
// when ScopedRlzValueStoreLock goes out of scope. Hence, if several
// ScopedRlzValueStoreLocks are nested, they all need to use the same store
// object.

RecursiveCrossProcessLock g_recursive_lock =
    RECURSIVE_CROSS_PROCESS_LOCK_INITIALIZER;

// This counts the nesting depth of |ScopedRlzValueStoreLock|.
int g_lock_depth = 0;

// This is the shared store object. Non-|NULL| only when |g_lock_depth > 0|.
RlzValueStoreChromeOS* g_store = NULL;

}  // namespace

ScopedRlzValueStoreLock::ScopedRlzValueStoreLock() {
  bool got_cross_process_lock =
      g_recursive_lock.TryGetCrossProcessLock(GetRlzStoreLockPath());
  // At this point, we hold the in-process lock, no matter the value of
  // |got_cross_process_lock|.

  ++g_lock_depth;
  if (!got_cross_process_lock) {
    // Acquiring cross-process lock failed, so simply return here.
    // In-process lock will be released in dtor.
    DCHECK(!g_store);
    return;
  }

  if (g_lock_depth > 1) {
    // Reuse the already existing store object.
    DCHECK(g_store);
    store_.reset(g_store);
    return;
  }

  // This is the topmost lock, create a new store object.
  DCHECK(!g_store);
  g_store = new RlzValueStoreChromeOS(GetRlzStorePath());
  store_.reset(g_store);
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
  --g_lock_depth;
  DCHECK_GE(g_lock_depth, 0);

  if (g_lock_depth > 0) {
    // Other locks are still using store_, so don't free it yet.
    ignore_result(store_.release());
    return;
  }

  g_store = NULL;

  g_recursive_lock.ReleaseLock();
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_.get();
}

namespace testing {

void SetRlzStoreDirectory(const base::FilePath& directory) {
  g_testing_rlz_store_path.Get() = directory;
}

std::string RlzStoreFilenameStr() {
  return GetRlzStorePath().value();
}

}  // namespace testing

}  // namespace rlz_lib
