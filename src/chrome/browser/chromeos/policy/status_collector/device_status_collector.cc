// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/enterprise_reporting.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::Time;
using base::TimeDelta;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const int kIdleStateThresholdSeconds = 300;

// How much time in the past to store active periods for.
constexpr TimeDelta kMaxStoredPastActivityInterval = TimeDelta::FromDays(30);

// How much time in the future to store active periods for.
constexpr TimeDelta kMaxStoredFutureActivityInterval = TimeDelta::FromDays(2);

// How often, in seconds, to sample the hardware resource usage.
const unsigned int kResourceUsageSampleIntervalSeconds = 120;

// The location we read our CPU statistics from.
const char kProcStat[] = "/proc/stat";

// The location we read our CPU temperature and channel label from.
const char kHwmonDir[] = "/sys/class/hwmon/";
const char kDeviceDir[] = "device";
const char kHwmonDirectoryPattern[] = "hwmon*";
const char kCPUTempFilePattern[] = "temp*_input";

// Activity periods are keyed with day and user in format:
// '<day_timestamp>:<BASE64 encoded user email>'
constexpr char kActivityKeySeparator = ':';
  
// The location where storage device statistics are read from.
const char kStorageInfoPath[] = "/var/log/storage_info.txt";

// How often the child's usage time is stored.
static constexpr base::TimeDelta kUpdateChildActiveTimeInterval =
    base::TimeDelta::FromSeconds(30);

// Helper function (invoked via blocking pool) to fetch information about
// mounted disks.
std::vector<em::VolumeInfo> GetVolumeInfo(
    const std::vector<std::string>& mount_points) {
  std::vector<em::VolumeInfo> result;
  for (const std::string& mount_point : mount_points) {
    base::FilePath mount_path(mount_point);

    // Non-native file systems do not have a mount point in the local file
    // system. However, it's worth checking here, as it's easier than checking
    // earlier which mount point is local, and which one is not.
    if (mount_point.empty() || !base::PathExists(mount_path))
      continue;

    int64_t free_size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    if (free_size < 0 || total_size < 0) {
      LOG(ERROR) << "Unable to get volume status for " << mount_point;
      continue;
    }
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    info.set_storage_total(total_size);
    info.set_storage_free(free_size);
    result.push_back(info);
  }
  return result;
}

// Reads the first CPU line from /proc/stat. Returns an empty string if
// the cpu data could not be read.
// The format of this line from /proc/stat is:
//
//   cpu  user_time nice_time system_time idle_time
//
// where user_time, nice_time, system_time, and idle_time are all integer
// values measured in jiffies from system startup.
std::string ReadCPUStatistics() {
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    size_t eol = contents.find("\n");
    if (eol != std::string::npos) {
      std::string line = contents.substr(0, eol);
      if (line.compare(0, 4, "cpu ") == 0)
        return line;
    }
    // First line should always start with "cpu ".
    NOTREACHED() << "Could not parse /proc/stat contents: " << contents;
  }
  LOG(WARNING) << "Unable to read CPU statistics from " << kProcStat;
  return std::string();
}

// Read system temperature sensors from
//
// /sys/class/hwmon/hwmon*/(device/)?temp*_input
//
// which contains millidegree Celsius temperature and
//
// /sys/class/hwmon/hwmon*/(device/)?temp*_label or
// /sys/class/hwmon/hwmon*/name
//
// which contains an appropriate label name for the given sensor.
std::vector<em::CPUTempInfo> ReadCPUTempInfo() {
  std::vector<em::CPUTempInfo> contents;
  // Get directories /sys/class/hwmon/hwmon*
  base::FileEnumerator hwmon_enumerator(base::FilePath(kHwmonDir), false,
                                        base::FileEnumerator::DIRECTORIES,
                                        kHwmonDirectoryPattern);

  for (base::FilePath hwmon_path = hwmon_enumerator.Next(); !hwmon_path.empty();
       hwmon_path = hwmon_enumerator.Next()) {
    // Get temp*_input files in hwmon*/ and hwmon*/device/
    if (base::PathExists(hwmon_path.Append(kDeviceDir))) {
      hwmon_path = hwmon_path.Append(kDeviceDir);
    }
    base::FileEnumerator enumerator(
        hwmon_path, false, base::FileEnumerator::FILES, kCPUTempFilePattern);
    for (base::FilePath temperature_path = enumerator.Next();
         !temperature_path.empty(); temperature_path = enumerator.Next()) {
      // Get appropriate temp*_label file.
      std::string label_path = temperature_path.MaybeAsASCII();
      if (label_path.empty()) {
        LOG(WARNING) << "Unable to parse a path to temp*_input file as ASCII";
        continue;
      }
      base::ReplaceSubstringsAfterOffset(&label_path, 0, "input", "label");
      base::FilePath name_path = hwmon_path.Append("name");

      // Get the label describing this temperature. Use temp*_label
      // if present, fall back on name file or blank.
      std::string label;
      if (base::PathExists(base::FilePath(label_path))) {
        base::ReadFileToString(base::FilePath(label_path), &label);
      } else if (base::PathExists(base::FilePath(name_path))) {
        base::ReadFileToString(name_path, &label);
      } else {
        label = std::string();
      }

      // Read temperature in millidegree Celsius.
      std::string temperature_string;
      int32_t temperature = 0;
      if (base::ReadFileToString(temperature_path, &temperature_string) &&
          sscanf(temperature_string.c_str(), "%d", &temperature) == 1) {
        // CPU temp in millidegree Celsius to Celsius
        temperature /= 1000;

        em::CPUTempInfo info;
        info.set_cpu_label(label);
        info.set_cpu_temp(temperature);
        contents.push_back(info);
      } else {
        LOG(WARNING) << "Unable to read CPU temp from "
                     << temperature_path.MaybeAsASCII();
      }
    }
  }
  return contents;
}

// If |contents| contains |prefix| followed by a hex integer, parses the hex
// integer of specified length and returns it.
// Otherwise, returns base::nullopt.
base::Optional<int> ExtractHexIntegerAfterPrefix(base::StringPiece contents,
                                                 base::StringPiece prefix,
                                                 size_t hex_number_length) {
  size_t prefix_position = contents.find(prefix);
  if (prefix_position == std::string::npos)
    return base::nullopt;
  if (prefix_position + prefix.size() + hex_number_length >= contents.size())
    return base::nullopt;
  int parsed_number;
  if (!base::HexStringToInt(
          contents.substr(prefix_position + prefix.size(), hex_number_length),
          &parsed_number)) {
    return base::nullopt;
  }
  return parsed_number;
}

// Read life time estimation value for eMMC from data generated by
// chromeos_disk_metrics. The data is stored in format:
// [DEVICE_LIFE_TIME_EST_TYP_[AB]: 0xXX]
// where A, B indicate the area of MMC being assesed(SLC and MLC), XX -- hex
// integer representing wear out of selected area.
// reference: e.MMC Device Health Report
// https://www.micron.com/products/managed-nand/emmc/emmc-software
em::DiskLifetimeEstimation ReadDiskLifeTimeEstimation() {
  em::DiskLifetimeEstimation est;
  std::string contents;
  const std::string pattern_slc = "[DEVICE_LIFE_TIME_EST_TYP_A: 0x";
  const std::string pattern_mlc = "[DEVICE_LIFE_TIME_EST_TYP_B: 0x";
  if (!base::ReadFileToStringWithMaxSize(
          base::FilePath(kStorageInfoPath), &contents,
          40000)) {  // max size in case somebody tackles with the file
    return est;
  }
  auto slc_est = ExtractHexIntegerAfterPrefix(contents, pattern_slc, 2);
  if (slc_est)
    est.set_slc(slc_est.value());
  auto mlc_est = ExtractHexIntegerAfterPrefix(contents, pattern_mlc, 2);
  if (mlc_est)
    est.set_mlc(mlc_est.value());
  return est;
}

bool ReadAndroidStatus(
    const policy::DeviceStatusCollector::AndroidStatusReceiver& receiver) {
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  auto* const instance_holder =
      arc_service_manager->arc_bridge_service()->enterprise_reporting();
  if (!instance_holder)
    return false;
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_holder, GetStatus);
  if (!instance)
    return false;
  instance->GetStatus(receiver);
  return true;
}

// Converts the given GetTpmStatusReply to TpmStatusInfo.
policy::TpmStatusInfo GetTpmStatusReplyToTpmStatusInfo(
    const base::Optional<cryptohome::BaseReply>& reply) {
  policy::TpmStatusInfo tpm_status_info;

  if (!reply.has_value()) {
    LOG(ERROR) << "GetTpmStatus call failed with empty reply.";
    return tpm_status_info;
  }
  if (reply->has_error() &&
      reply->error() != cryptohome::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "GetTpmStatus failed with error: " << reply->error();
    return tpm_status_info;
  }
  if (!reply->HasExtension(cryptohome::GetTpmStatusReply::reply)) {
    LOG(ERROR)
        << "GetTpmStatus failed with no GetTpmStatusReply extension in reply.";
    return tpm_status_info;
  }

  auto reply_proto = reply->GetExtension(cryptohome::GetTpmStatusReply::reply);

  tpm_status_info.enabled = reply_proto.enabled();
  tpm_status_info.owned = reply_proto.owned();
  tpm_status_info.initialized = reply_proto.initialized();
  tpm_status_info.attestation_prepared = reply_proto.attestation_prepared();
  tpm_status_info.attestation_enrolled = reply_proto.attestation_enrolled();
  tpm_status_info.dictionary_attack_counter =
      reply_proto.dictionary_attack_counter();
  tpm_status_info.dictionary_attack_threshold =
      reply_proto.dictionary_attack_threshold();
  tpm_status_info.dictionary_attack_lockout_in_effect =
      reply_proto.dictionary_attack_lockout_in_effect();
  tpm_status_info.dictionary_attack_lockout_seconds_remaining =
      reply_proto.dictionary_attack_lockout_seconds_remaining();
  tpm_status_info.boot_lockbox_finalized = reply_proto.boot_lockbox_finalized();

  return tpm_status_info;
}

void ReadTpmStatus(policy::DeviceStatusCollector::TpmStatusReceiver callback) {
  // D-Bus calls are allowed only on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::CryptohomeClient::Get()->GetTpmStatus(
      cryptohome::GetTpmStatusRequest(),
      base::BindOnce(
          [](policy::DeviceStatusCollector::TpmStatusReceiver callback,
             base::Optional<cryptohome::BaseReply> reply) {
            std::move(callback).Run(GetTpmStatusReplyToTpmStatusInfo(reply));
          },
          std::move(callback)));
}

base::Version GetPlatformVersion() {
  return base::Version(base::SysInfo::OperatingSystemVersion());
}

// Helper routine to convert from Shill-provided signal strength (percent)
// to dBm units expected by server.
int ConvertWifiSignalStrength(int signal_strength) {
  // Shill attempts to convert WiFi signal strength from its internal dBm to a
  // percentage range (from 0-100) by adding 120 to the raw dBm value,
  // and then clamping the result to the range 0-100 (see
  // shill::WiFiService::SignalToStrength()).
  //
  // To convert back to dBm, we subtract 120 from the percentage value to yield
  // a clamped dBm value in the range of -119 to -20dBm.
  //
  // TODO(atwilson): Tunnel the raw dBm signal strength from Shill instead of
  // doing the conversion here so we can report non-clamped values
  // (crbug.com/463334).
  DCHECK_GT(signal_strength, 0);
  DCHECK_LE(signal_strength, 100);
  return signal_strength - 120;
}

bool IsKioskApp() {
  auto user_type = chromeos::LoginState::Get()->GetLoggedInUserType();
  return user_type == chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP ||
         user_type == chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP;
}

// Utility method to turn cpu_temp_fetcher_ to OnceCallback
std::vector<em::CPUTempInfo> InvokeCpuTempFetcher(
    policy::DeviceStatusCollector::CPUTempFetcher fetcher) {
  return fetcher.Run();
}

}  // namespace

namespace policy {

// Helper class for state tracking of async status queries. Creates device and
// session status blobs in the constructor and sends them to the the status
// response callback in the destructor.
//
// Some methods like |SampleVolumeInfo| queue async queries to collect data. The
// response callback of these queries, e.g. |OnVolumeInfoReceived|, holds a
// reference to the instance of this class, so that the destructor will not be
// invoked and the status response callback will not be fired until the original
// owner of the instance releases its reference and all async queries finish.
//
// Therefore, if you create an instance of this class, make sure to release your
// reference after quering all async queries (if any), e.g. by using a local
// |scoped_refptr<GetStatusState>| and letting it go out of scope.
class GetStatusState : public base::RefCountedThreadSafe<GetStatusState> {
 public:
  explicit GetStatusState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      const policy::StatusCollectorCallback& response)
      : task_runner_(task_runner), response_(response) {}

  inline em::DeviceStatusReportRequest* device_status() {
    return response_params_.device_status.get();
  }

  inline em::SessionStatusReportRequest* session_status() {
    return response_params_.session_status.get();
  }

  inline void ResetDeviceStatus() { response_params_.device_status.reset(); }

  inline void ResetSessionStatus() { response_params_.session_status.reset(); }

  // Queues an async callback to query disk volume information.
  void SampleVolumeInfo(const policy::DeviceStatusCollector::VolumeInfoFetcher&
                            volume_info_fetcher) {
    // Create list of mounted disk volumes to query status.
    std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
    storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
        &external_mount_points);

    std::vector<std::string> mount_points;
    for (const auto& info : external_mount_points)
      mount_points.push_back(info.path.value());

    for (const auto& mount_info :
         chromeos::disks::DiskMountManager::GetInstance()->mount_points()) {
      // Extract a list of mount points to populate.
      mount_points.push_back(mount_info.first);
    }

    // Call out to the blocking pool to sample disk volume info.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(volume_info_fetcher, mount_points),
        base::Bind(&GetStatusState::OnVolumeInfoReceived, this));
  }

  // Queues an async callback to query CPU temperature information.
  void SampleCPUTempInfo(
      const policy::DeviceStatusCollector::CPUTempFetcher& cpu_temp_fetcher) {
    // Call out to the blocking pool to sample CPU temp.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        cpu_temp_fetcher,
        base::Bind(&GetStatusState::OnCPUTempInfoReceived, this));
  }

  bool FetchAndroidStatus(
      const policy::DeviceStatusCollector::AndroidStatusFetcher&
          android_status_fetcher) {
    return android_status_fetcher.Run(
        base::Bind(&GetStatusState::OnAndroidInfoReceived, this));
  }

  void FetchTpmStatus(const policy::DeviceStatusCollector::TpmStatusFetcher&
                          tpm_status_fetcher) {
    tpm_status_fetcher.Run(
        base::BindOnce(&GetStatusState::OnTpmStatusReceived, this));
  }

  void FetchProbeData(const policy::DeviceStatusCollector::ProbeDataFetcher&
                          probe_data_fetcher) {
    probe_data_fetcher.Run(
        base::BindOnce(&GetStatusState::OnProbeDataReceived, this));
  }

  void FetchEMMCLifeTime(
      const policy::DeviceStatusCollector::EMMCLifetimeFetcher&
          emmc_lifetime_fetcher) {
    // Call out to the blocking pool to read disklifetimeestimation.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(emmc_lifetime_fetcher),
        base::BindOnce(&GetStatusState::OnEMMCLifetimeReceived,
                       this));
  }

 private:
  friend class RefCountedThreadSafe<GetStatusState>;

  // Posts the response on the UI thread. As long as there is an outstanding
  // async query, the query holds a reference to us, so the destructor is
  // not called.
  ~GetStatusState() {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(response_, base::Passed(&response_params_)));
  }

  void OnVolumeInfoReceived(const std::vector<em::VolumeInfo>& volume_info) {
    response_params_.device_status->clear_volume_infos();
    for (const em::VolumeInfo& info : volume_info)
      *response_params_.device_status->add_volume_infos() = info;
  }

  void OnCPUTempInfoReceived(
      const std::vector<em::CPUTempInfo>& cpu_temp_info) {
    // Only one of OnProbeDataReceived and OnCPUTempInfoReceived should be
    // called.
    DCHECK(response_params_.device_status->cpu_temp_infos_size() == 0);

    DLOG_IF(WARNING, cpu_temp_info.empty())
        << "Unable to read CPU temp information.";
    base::Time timestamp = base::Time::Now();
    for (const em::CPUTempInfo& info : cpu_temp_info) {
      auto* new_info = response_params_.device_status->add_cpu_temp_infos();
      *new_info = info;
      new_info->set_timestamp(timestamp.ToJavaTime());
    }
  }

  void OnAndroidInfoReceived(const std::string& status,
                             const std::string& droid_guard_info) {
    em::AndroidStatus* const android_status =
        response_params_.session_status->mutable_android_status();
    android_status->set_status_payload(status);
    android_status->set_droid_guard_info(droid_guard_info);
  }

  void OnTpmStatusReceived(const TpmStatusInfo& tpm_status_struct) {
    // Make sure we edit the state on the right thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    em::TpmStatusInfo* const tpm_status_proto =
        response_params_.device_status->mutable_tpm_status_info();

    tpm_status_proto->set_enabled(tpm_status_struct.enabled);
    tpm_status_proto->set_owned(tpm_status_struct.owned);
    tpm_status_proto->set_tpm_initialized(tpm_status_struct.initialized);
    tpm_status_proto->set_attestation_prepared(
        tpm_status_struct.attestation_prepared);
    tpm_status_proto->set_attestation_enrolled(
        tpm_status_struct.attestation_enrolled);
    tpm_status_proto->set_dictionary_attack_counter(
        tpm_status_struct.dictionary_attack_counter);
    tpm_status_proto->set_dictionary_attack_threshold(
        tpm_status_struct.dictionary_attack_threshold);
    tpm_status_proto->set_dictionary_attack_lockout_in_effect(
        tpm_status_struct.dictionary_attack_lockout_in_effect);
    tpm_status_proto->set_dictionary_attack_lockout_seconds_remaining(
        tpm_status_struct.dictionary_attack_lockout_seconds_remaining);
    tpm_status_proto->set_boot_lockbox_finalized(
        tpm_status_struct.boot_lockbox_finalized);
  }

  void OnProbeDataReceived(
      const base::Optional<runtime_probe::ProbeResult>& probe_result,
      const base::circular_deque<std::unique_ptr<SampledData>>& samples) {
    // Make sure we edit the state on the right thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Only one of OnProbeDataReceived and OnCPUTempInfoReceived should be
    // called.
    DCHECK(response_params_.device_status->cpu_temp_infos_size() == 0);

    // Store CPU measurement samples.
    for (const std::unique_ptr<SampledData>& sample_data : samples) {
      for (auto it = sample_data->cpu_samples.begin();
           it != sample_data->cpu_samples.end(); it++) {
        auto* new_info = response_params_.device_status->add_cpu_temp_infos();
        *new_info = it->second;
      }
    }

    if (!probe_result.has_value())
      return;
    if (probe_result.value().error() !=
        runtime_probe::RUNTIME_PROBE_ERROR_NOT_SET) {
      return;
    }
    if (probe_result.value().battery_size() > 0) {
      em::PowerStatus* const power_status =
          response_params_.device_status->mutable_power_status();
      for (const auto& battery : probe_result.value().battery()) {
        em::BatteryInfo* const battery_info = power_status->add_batteries();
        battery_info->set_serial(battery.values().serial_number());
        battery_info->set_manufacturer(battery.values().manufacturer());
        battery_info->set_cycle_count(battery.values().cycle_count_smart());
        // uAh to mAh
        battery_info->set_design_capacity(
            battery.values().charge_full_design() / 1000);
        battery_info->set_full_charge_capacity(battery.values().charge_full() /
                                               1000);
        // uV to mV:
        battery_info->set_design_min_voltage(
            battery.values().voltage_min_design() / 1000);

        for (const std::unique_ptr<SampledData>& sample_data : samples) {
          auto it = sample_data->battery_samples.find(battery.name());
          if (it != sample_data->battery_samples.end())
            battery_info->add_samples()->CheckTypeAndMergeFrom(it->second);
        }
      }
    }
    if (probe_result.value().storage_size() > 0) {
      em::StorageStatus* const storage_status =
          response_params_.device_status->mutable_storage_status();
      for (const auto& storage : probe_result.value().storage()) {
        em::DiskInfo* const disk_info = storage_status->add_disks();
        disk_info->set_serial(base::NumberToString(storage.values().serial()));
        disk_info->set_manufacturer(
            base::NumberToString(storage.values().manfid()));
        disk_info->set_model(storage.values().name());
        disk_info->set_type(storage.values().type());
        disk_info->set_size(storage.values().size());
      }
    }
  }

  void OnEMMCLifetimeReceived(const em::DiskLifetimeEstimation& est) {
    if (!est.has_slc() && !est.has_mlc())
      return;
    em::DiskLifetimeEstimation* state =
        response_params_.device_status->mutable_storage_status()
            ->mutable_lifetime_estimation();
    state->CopyFrom(est);
  }
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  policy::StatusCollectorCallback response_;
  StatusCollectorParams response_params_;
};

// Handles storing activity time periods needed for reporting. Provides
// filtering of the user identifying data.
class DeviceStatusCollector::ActivityStorage {
 public:
  // Stored activity period.
  struct ActivityPeriod {
    // Email can be empty.
    std::string user_email;

    // Timestamp dating the beginning of the captured activity.
    int64_t start_timestamp;

    // User's activity in milliseconds.
    int activity_milliseconds;
  };

  // Creates activity storage. Activity data will be stored in the given
  // |pref_service| under |pref_name| preference. Activity data are aggregated
  // by day. The start of the new day is counted from |activity_day_start| that
  // represents the distance from midnight.
  ActivityStorage(PrefService* pref_service,
                  const std::string& pref_name,
                  TimeDelta activity_day_start,
                  bool is_enterprise_reporting);
  ~ActivityStorage();

  // Adds an activity period. Accepts empty |active_user_email| if it should not
  // be stored.
  void AddActivityPeriod(Time start,
                         Time end,
                         Time now,
                         const std::string& active_user_email);

  // Clears stored activity periods outside of storage range defined by
  // |max_past_activity_interval| and |max_future_activity_interval| from
  // |base_time|.
  void PruneActivityPeriods(Time base_time,
                            TimeDelta max_past_activity_interval,
                            TimeDelta max_future_activity_interval);

  // Trims the store activity periods to only retain data within the
  // [|min_day_key|, |max_day_key|). The record for |min_day_key| will be
  // adjusted by subtracting |min_day_trim_duration|.
  void TrimActivityPeriods(int64_t min_day_key,
                           int min_day_trim_duration,
                           int64_t max_day_key);

  // Updates stored activity period according to users' reporting preferences.
  // Removes user's email and aggregates the activity data if user's information
  // should no longer be reported.
  void FilterActivityPeriodsByUsers(
      const std::vector<std::string>& reporting_users);

  // Returns the list of stored activity periods. Aggregated data are returned
  // without email addresses if |omit_emails| is set.
  std::vector<ActivityPeriod> GetFilteredActivityPeriods(bool omit_emails);

 private:
  static std::string MakeActivityPeriodPrefKey(int64_t start,
                                               const std::string& user_email);
  static bool ParseActivityPeriodPrefKey(const std::string& key,
                                         int64_t* start_timestamp,
                                         std::string* user_email);
  void ProcessActivityPeriods(const base::DictionaryValue& activity_times,
                              const std::vector<std::string>& reporting_users,
                              base::DictionaryValue* const filtered_times);
  void StoreChildScreenTime(Time activity_day_start,
                            TimeDelta activity,
                            Time now);

  // Determine the day key (milliseconds since epoch for corresponding
  // |day_start_| in UTC) for a given |timestamp|.
  int64_t TimestampToDayKey(Time timestamp);

  PrefService* const pref_service_ = nullptr;
  const std::string pref_name_;

  // New day start time used for aggregating data represented by the distance
  // from midnight.
  const TimeDelta day_start_;

  // Whether reporting is for enterprise or consumer.
  bool is_enterprise_reporting_ = false;

  DISALLOW_COPY_AND_ASSIGN(ActivityStorage);
};

TpmStatusInfo::TpmStatusInfo() = default;
TpmStatusInfo::TpmStatusInfo(const TpmStatusInfo&) = default;
TpmStatusInfo::TpmStatusInfo(
    bool enabled,
    bool owned,
    bool initialized,
    bool attestation_prepared,
    bool attestation_enrolled,
    int32_t dictionary_attack_counter,
    int32_t dictionary_attack_threshold,
    bool dictionary_attack_lockout_in_effect,
    int32_t dictionary_attack_lockout_seconds_remaining,
    bool boot_lockbox_finalized)
    : enabled(enabled),
      owned(owned),
      initialized(initialized),
      attestation_prepared(attestation_prepared),
      attestation_enrolled(attestation_enrolled),
      dictionary_attack_counter(dictionary_attack_counter),
      dictionary_attack_threshold(dictionary_attack_threshold),
      dictionary_attack_lockout_in_effect(dictionary_attack_lockout_in_effect),
      dictionary_attack_lockout_seconds_remaining(
          dictionary_attack_lockout_seconds_remaining),
      boot_lockbox_finalized(boot_lockbox_finalized) {}
TpmStatusInfo::~TpmStatusInfo() = default;

DeviceStatusCollector::ActivityStorage::ActivityStorage(
    PrefService* pref_service,
    const std::string& pref_name,
    TimeDelta activity_day_start,
    bool is_enterprise_reporting)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      day_start_(activity_day_start),
      is_enterprise_reporting_(is_enterprise_reporting) {
  DCHECK(pref_service_);
  const PrefService::PrefInitializationStatus pref_service_status =
      pref_service_->GetInitializationStatus();
  DCHECK(pref_service_status != PrefService::INITIALIZATION_STATUS_WAITING &&
         pref_service_status != PrefService::INITIALIZATION_STATUS_ERROR);
}

DeviceStatusCollector::ActivityStorage::~ActivityStorage() = default;

void DeviceStatusCollector::ActivityStorage::AddActivityPeriod(
    Time start,
    Time end,
    Time now,
    const std::string& active_user_email) {
  DCHECK(start <= end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  Time day_start = start.LocalMidnight() + day_start_;
  if (start < day_start)
    day_start -= TimeDelta::FromDays(1);
  while (day_start < end) {
    day_start += TimeDelta::FromDays(1);
    int64_t activity = (std::min(end, day_start) - start).InMilliseconds();
    const std::string key =
        MakeActivityPeriodPrefKey(TimestampToDayKey(start), active_user_email);
    int previous_activity = 0;
    activity_times->GetInteger(key, &previous_activity);
    activity_times->SetInteger(key, previous_activity + activity);

    // If the user is a child, the child screen time pref may need to be
    // updated.
    if (user_manager::UserManager::Get()->IsLoggedInAsChildUser() &&
        !is_enterprise_reporting_) {
      StoreChildScreenTime(day_start - TimeDelta::FromDays(1),
                           TimeDelta::FromMilliseconds(activity), now);
    }

    start = day_start;
  }
}

void DeviceStatusCollector::ActivityStorage::PruneActivityPeriods(
    Time base_time,
    TimeDelta max_past_activity_interval,
    TimeDelta max_future_activity_interval) {
  Time min_time = base_time - max_past_activity_interval;
  Time max_time = base_time + max_future_activity_interval;
  TrimActivityPeriods(TimestampToDayKey(min_time), 0,
                      TimestampToDayKey(max_time));
}

void DeviceStatusCollector::ActivityStorage::TrimActivityPeriods(
    int64_t min_day_key,
    int min_day_trim_duration,
    int64_t max_day_key) {
  const base::DictionaryValue* activity_times =
      pref_service_->GetDictionary(pref_name_);

  std::unique_ptr<base::DictionaryValue> copy(activity_times->DeepCopy());
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t timestamp;
    std::string active_user_email;
    if (ParseActivityPeriodPrefKey(it.key(), &timestamp, &active_user_email)) {
      // Remove data that is too old, or too far in the future.
      if (timestamp >= min_day_key && timestamp < max_day_key) {
        if (timestamp == min_day_key) {
          int new_activity_duration = 0;
          if (it.value().GetAsInteger(&new_activity_duration)) {
            new_activity_duration =
                std::max(new_activity_duration - min_day_trim_duration, 0);
          }
          copy->SetInteger(it.key(), new_activity_duration);
        }
        continue;
      }
    }
    // The entry is out of range or couldn't be parsed. Remove it.
    copy->Remove(it.key(), NULL);
  }
  pref_service_->Set(pref_name_, *copy);
}

void DeviceStatusCollector::ActivityStorage::FilterActivityPeriodsByUsers(
    const std::vector<std::string>& reporting_users) {
  const base::DictionaryValue* stored_activity_periods =
      pref_service_->GetDictionary(pref_name_);
  base::DictionaryValue filtered_activity_periods;
  ProcessActivityPeriods(*stored_activity_periods, reporting_users,
                         &filtered_activity_periods);
  pref_service_->Set(pref_name_, filtered_activity_periods);
}

std::vector<DeviceStatusCollector::ActivityStorage::ActivityPeriod>
DeviceStatusCollector::ActivityStorage::GetFilteredActivityPeriods(
    bool omit_emails) {
  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* stored_activity_periods = update.Get();

  base::DictionaryValue filtered_activity_periods;
  if (omit_emails) {
    std::vector<std::string> empty_user_list;
    ProcessActivityPeriods(*stored_activity_periods, empty_user_list,
                           &filtered_activity_periods);
    stored_activity_periods = &filtered_activity_periods;
  }

  std::vector<ActivityPeriod> activity_periods;
  for (base::DictionaryValue::Iterator it(*stored_activity_periods);
       !it.IsAtEnd(); it.Advance()) {
    ActivityPeriod activity_period;
    if (ParseActivityPeriodPrefKey(it.key(), &activity_period.start_timestamp,
                                   &activity_period.user_email) &&
        it.value().GetAsInteger(&activity_period.activity_milliseconds)) {
      activity_periods.push_back(activity_period);
    }
  }
  return activity_periods;
}

// static
std::string DeviceStatusCollector::ActivityStorage::MakeActivityPeriodPrefKey(
    int64_t start,
    const std::string& user_email) {
  const std::string day_key = base::NumberToString(start);
  if (user_email.empty())
    return day_key;

  std::string encoded_email;
  base::Base64Encode(user_email, &encoded_email);
  return day_key + kActivityKeySeparator + encoded_email;
}

// static
bool DeviceStatusCollector::ActivityStorage::ParseActivityPeriodPrefKey(
    const std::string& key,
    int64_t* start_timestamp,
    std::string* user_email) {
  auto separator_pos = key.find(kActivityKeySeparator);
  if (separator_pos == std::string::npos) {
    user_email->clear();
    return base::StringToInt64(key, start_timestamp);
  }
  return base::StringToInt64(key.substr(0, separator_pos), start_timestamp) &&
         base::Base64Decode(key.substr(separator_pos + 1), user_email);
}

void DeviceStatusCollector::ActivityStorage::ProcessActivityPeriods(
    const base::DictionaryValue& activity_times,
    const std::vector<std::string>& reporting_users,
    base::DictionaryValue* const filtered_times) {
  std::set<std::string> reporting_users_set(reporting_users.begin(),
                                            reporting_users.end());
  const std::string empty;
  for (const auto& it : activity_times.DictItems()) {
    DCHECK(it.second.is_int());
    int64_t timestamp;
    std::string user_email;
    if (!ParseActivityPeriodPrefKey(it.first, &timestamp, &user_email))
      continue;
    if (!user_email.empty() && reporting_users_set.count(user_email) == 0) {
      int value = 0;
      std::string timestamp_str = MakeActivityPeriodPrefKey(timestamp, empty);
      const base::Value* prev_value = filtered_times->FindKeyOfType(
          timestamp_str, base::Value::Type::INTEGER);
      if (prev_value)
        value = prev_value->GetInt();
      filtered_times->SetKey(timestamp_str,
                             base::Value(value + it.second.GetInt()));
    } else {
      filtered_times->SetKey(it.first, it.second.Clone());
    }
  }
}

int64_t DeviceStatusCollector::ActivityStorage::TimestampToDayKey(
    Time timestamp) {
  Time::Exploded exploded;
  Time day_start = timestamp.LocalMidnight() + day_start_;
  if (timestamp < day_start)
    day_start -= TimeDelta::FromDays(1);
  day_start.LocalExplode(&exploded);
  Time out_time;
  bool conversion_success = Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time.ToJavaTime();
}

void DeviceStatusCollector::ActivityStorage::StoreChildScreenTime(
    Time activity_day_start,
    TimeDelta activity,
    Time now) {
  DCHECK(user_manager::UserManager::Get()->IsLoggedInAsChildUser() &&
         !is_enterprise_reporting_);

  // Today's start time.
  Time today_start = now.LocalMidnight() + day_start_;
  if (today_start > now)
    today_start -= TimeDelta::FromDays(1);

  // The activity windows always start and end on the reset time of two
  // consecutive days, so it is not possible to have a window starting after
  // the current day's reset time.
  DCHECK(activity_day_start <= today_start);

  TimeDelta previous_activity = TimeDelta::FromMilliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));

  // If this activity window belongs to the current day, the screen time pref
  // should be updated.
  if (activity_day_start == today_start) {
    pref_service_->SetInteger(prefs::kChildScreenTimeMilliseconds,
                              (previous_activity + activity).InMilliseconds());
    pref_service_->SetTime(prefs::kLastChildScreenTimeSaved, now);
  }
  pref_service_->CommitPendingWrite();
}

SampledData::SampledData() = default;
SampledData::~SampledData() = default;

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* pref_service,
    chromeos::system::StatisticsProvider* provider,
    const VolumeInfoFetcher& volume_info_fetcher,
    const CPUStatisticsFetcher& cpu_statistics_fetcher,
    const CPUTempFetcher& cpu_temp_fetcher,
    const AndroidStatusFetcher& android_status_fetcher,
    const TpmStatusFetcher& tpm_status_fetcher,
    const EMMCLifetimeFetcher& emmc_lifetime_fetcher,
    TimeDelta activity_day_start,
    bool is_enterprise_reporting)
    : max_stored_past_activity_interval_(kMaxStoredPastActivityInterval),
      max_stored_future_activity_interval_(kMaxStoredFutureActivityInterval),
      pref_service_(pref_service),
      last_idle_check_(Time()),
      last_active_check_(base::Time()),
      last_state_active_(true),
      volume_info_fetcher_(volume_info_fetcher),
      cpu_statistics_fetcher_(cpu_statistics_fetcher),
      cpu_temp_fetcher_(cpu_temp_fetcher),
      android_status_fetcher_(android_status_fetcher),
      tpm_status_fetcher_(tpm_status_fetcher),
      emmc_lifetime_fetcher_(emmc_lifetime_fetcher),
      statistics_provider_(provider),
      cros_settings_(chromeos::CrosSettings::Get()),
      power_manager_(chromeos::PowerManagerClient::Get()),
      session_manager_(session_manager::SessionManager::Get()),
      runtime_probe_(
          chromeos::DBusThreadManager::Get()->GetRuntimeProbeClient()),
      is_enterprise_reporting_(is_enterprise_reporting),
      activity_day_start_(activity_day_start),
      task_runner_(nullptr),
      weak_factory_(this) {
  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_ = base::SequencedTaskRunnerHandle::Get();

  if (volume_info_fetcher_.is_null())
    volume_info_fetcher_ = base::Bind(&GetVolumeInfo);

  if (cpu_statistics_fetcher_.is_null())
    cpu_statistics_fetcher_ = base::Bind(&ReadCPUStatistics);

  if (cpu_temp_fetcher_.is_null())
    cpu_temp_fetcher_ = base::Bind(&ReadCPUTempInfo);

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::Bind(&ReadAndroidStatus);

  if (tpm_status_fetcher_.is_null())
    tpm_status_fetcher_ = base::BindRepeating(&ReadTpmStatus);

  if (probe_data_fetcher_.is_null())
    probe_data_fetcher_ = base::BindRepeating(
        &DeviceStatusCollector::FetchProbeData, weak_factory_.GetWeakPtr());

  if (emmc_lifetime_fetcher_.is_null())
    emmc_lifetime_fetcher_ = base::BindRepeating(&ReadDiskLifeTimeEstimation);
  idle_poll_timer_.Start(FROM_HERE,
                         TimeDelta::FromSeconds(kIdlePollIntervalSeconds), this,
                         &DeviceStatusCollector::CheckIdleState);
  update_child_usage_timer_.Start(FROM_HERE, kUpdateChildActiveTimeInterval,
                                  this,
                                  &DeviceStatusCollector::UpdateChildUsageTime);
  resource_usage_sampling_timer_.Start(
      FROM_HERE, TimeDelta::FromSeconds(kResourceUsageSampleIntervalSeconds),
      this, &DeviceStatusCollector::SampleResourceUsage);

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  base::Closure callback = base::Bind(
      &DeviceStatusCollector::UpdateReportingSettings, base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVersionInfo, callback);
  activity_times_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceActivityTimes, callback);
  boot_mode_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBootMode, callback);
  network_interfaces_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceNetworkInterfaces, callback);
  users_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceUsers, callback);
  hardware_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceHardwareStatus, callback);
  session_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceSessionStatus, callback);
  os_update_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportOsUpdateStatus, callback);
  running_kiosk_app_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportRunningKioskApp, callback);
  power_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDevicePowerStatus, callback);
  storage_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceStorageStatus, callback);
  board_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBoardStatus, callback);

  // Watch for changes on the device state to calculate the child's active time.
  power_manager_->AddObserver(this);
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier)) {
    chromeos::UsageTimeStateNotifier::GetInstance()->AddObserver(this);
  } else {
    session_manager_->AddObserver(this);
  }

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS, firmware, and TPM version info.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&DeviceStatusCollector::OnOSVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&DeviceStatusCollector::OnOSFirmware,
                 weak_factory_.GetWeakPtr()));
  chromeos::tpm_util::GetTpmVersion(base::BindOnce(
      &DeviceStatusCollector::OnTpmVersion, weak_factory_.GetWeakPtr()));

  // If doing enterprise device-level reporting, observe the list of users to be
  // reported. Consumer reporting is enforced for the signed-in registered user
  // therefore this preference is not observed.
  if (is_enterprise_reporting_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        prefs::kReportingUsers,
        base::BindRepeating(&DeviceStatusCollector::ReportingUsersChanged,
                            weak_factory_.GetWeakPtr()));
  }

  DCHECK(pref_service_->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);
  activity_storage_ = std::make_unique<ActivityStorage>(
      pref_service_,
      (is_enterprise_reporting_ ? prefs::kDeviceActivityTimes
                                : prefs::kUserActivityTimes),
      activity_day_start, is_enterprise_reporting_);
}

DeviceStatusCollector::~DeviceStatusCollector() {
  power_manager_->RemoveObserver(this);
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier)) {
    chromeos::UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);
  } else {
    session_manager_->RemoveObserver(this);
  }
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceActivityTimes);
}

// static
void DeviceStatusCollector::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kReportArcStatusEnabled, false);
  registry->RegisterDictionaryPref(prefs::kUserActivityTimes);
  registry->RegisterTimePref(prefs::kLastChildScreenTimeReset, Time());
  registry->RegisterTimePref(prefs::kLastChildScreenTimeSaved, Time());
  registry->RegisterIntegerPref(prefs::kChildScreenTimeMilliseconds, 0);
}

TimeDelta DeviceStatusCollector::GetActiveChildScreenTime() {
  if (!user_manager::UserManager::Get()->IsLoggedInAsChildUser())
    return TimeDelta::FromSeconds(0);

  UpdateChildUsageTime();
  return TimeDelta::FromMilliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));
}

void DeviceStatusCollector::CheckIdleState() {
  ProcessIdleState(ui::CalculateIdleState(kIdleStateThresholdSeconds));
}

void DeviceStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceVersionInfo,
                                  &report_version_info_)) {
    report_version_info_ = true;
  }
  if (!is_enterprise_reporting_) {
    // Only report activity times for consumer if time limit is enabled.
    report_activity_times_ =
        base::FeatureList::IsEnabled(features::kUsageTimeLimitPolicy);
  } else if (!cros_settings_->GetBoolean(chromeos::kReportDeviceActivityTimes,
                                         &report_activity_times_)) {
    report_activity_times_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBootMode,
                                  &report_boot_mode_)) {
    report_boot_mode_ = true;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceSessionStatus,
                                  &report_kiosk_session_status_)) {
    report_kiosk_session_status_ = true;
  }
  // Network interfaces are reported for enterprise devices only by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceNetworkInterfaces,
                                  &report_network_interfaces_)) {
    report_network_interfaces_ = is_enterprise_reporting_;
  }
  // Device users are reported for enterprise devices only by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceUsers,
                                  &report_users_)) {
    report_users_ = is_enterprise_reporting_;
  }
  // Hardware status is reported for enterprise devices only by default.
  const bool already_reporting_hardware_status = report_hardware_status_;
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceHardwareStatus,
                                  &report_hardware_status_)) {
    report_hardware_status_ = is_enterprise_reporting_;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDevicePowerStatus,
                                  &report_power_status_)) {
    report_power_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceStorageStatus,
                                  &report_storage_status_)) {
    report_storage_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportDeviceBoardStatus,
                                  &report_board_status_)) {
    report_board_status_ = false;
  }

  if (!report_hardware_status_) {
    ClearCachedResourceUsage();
  } else if (!already_reporting_hardware_status) {
    // Turning on hardware status reporting - fetch an initial sample
    // immediately instead of waiting for the sampling timer to fire.
    SampleResourceUsage();
  }

  // Os update status and running kiosk app reporting are disabled by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportOsUpdateStatus,
                                  &report_os_update_status_)) {
    report_os_update_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportRunningKioskApp,
                                  &report_running_kiosk_app_)) {
    report_running_kiosk_app_ = false;
  }
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

void DeviceStatusCollector::ClearCachedResourceUsage() {
  resource_usage_.clear();
  last_cpu_active_ = 0;
  last_cpu_idle_ = 0;
}

void DeviceStatusCollector::ProcessIdleState(ui::IdleState state) {
  // Do nothing if device activity reporting is disabled or if it's a child
  // account. Usage time for child accounts are calculated differently.
  if (!report_activity_times_ || !is_enterprise_reporting_ ||
      user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    return;
  }

  Time now = GetCurrentTime();

  // For kiosk apps we report total uptime instead of active time.
  if (state == ui::IDLE_STATE_ACTIVE || IsKioskApp()) {
    CHECK(is_enterprise_reporting_);
    std::string user_email = GetUserForActivityReporting();
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    int active_seconds = (now - last_idle_check_).InSeconds();
    if (active_seconds < 0 ||
        active_seconds >= static_cast<int>((2 * kIdlePollIntervalSeconds))) {
      activity_storage_->AddActivityPeriod(
          now - TimeDelta::FromSeconds(kIdlePollIntervalSeconds), now, now,
          user_email);
    } else {
      activity_storage_->AddActivityPeriod(last_idle_check_, now, now,
                                           user_email);
    }

    activity_storage_->PruneActivityPeriods(
        now, max_stored_past_activity_interval_,
        max_stored_future_activity_interval_);
  }
  last_idle_check_ = now;
}

void DeviceStatusCollector::OnSessionStateChanged() {
  UpdateChildUsageTime();
  last_state_active_ =
      session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::ACTIVE;
}

void DeviceStatusCollector::OnUsageTimeStateChange(
    chromeos::UsageTimeStateNotifier::UsageTimeState state) {
  UpdateChildUsageTime();
  last_state_active_ =
      state == chromeos::UsageTimeStateNotifier::UsageTimeState::ACTIVE;
}

void DeviceStatusCollector::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  // This logic are going to be done by OnUsageTimeStateChange method if
  // UsageTimeStateNotifier feature is enabled.
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
    return;

  UpdateChildUsageTime();
  // It is active if screen is on and if the session is also active.
  last_state_active_ =
      !state.off() && session_manager_->session_state() ==
                          session_manager::SessionState::ACTIVE;
}

void DeviceStatusCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // This logic are going to be done by OnUsageTimeStateChange method if
  // UsageTimeStateNotifier feature is enabled.
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
    return;

  UpdateChildUsageTime();
  // Device is going to be suspeded, so it won't be active.
  last_state_active_ = false;
}

void DeviceStatusCollector::SuspendDone(const base::TimeDelta& sleep_duration) {
  // This logic are going to be done by OnUsageTimeStateChange method if
  // UsageTimeStateNotifier feature is enabled.
  if (base::FeatureList::IsEnabled(features::kUsageTimeStateNotifier))
    return;

  UpdateChildUsageTime();
  // Device is returning from suspension, so it is considered active if the
  // session is also active.
  last_state_active_ = session_manager_->session_state() ==
                       session_manager::SessionState::ACTIVE;
}

void DeviceStatusCollector::PowerChanged(
    const power_manager::PowerSupplyProperties& prop) {
  if (!power_status_callback_.is_null())
    std::move(power_status_callback_).Run(prop);
}

void DeviceStatusCollector::UpdateChildUsageTime() {
  if (!report_activity_times_ ||
      !user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    return;
  }

  // Only child accounts should be using this method.
  CHECK(user_manager::UserManager::Get()->IsLoggedInAsChildUser());

  Time now = GetCurrentTime();
  Time reset_time = now.LocalMidnight() + activity_day_start_;
  if (reset_time > now)
    reset_time -= TimeDelta::FromDays(1);
  // Reset screen time if it has not been reset today.
  if (reset_time > pref_service_->GetTime(prefs::kLastChildScreenTimeReset)) {
    pref_service_->SetTime(prefs::kLastChildScreenTimeReset, now);
    pref_service_->SetInteger(prefs::kChildScreenTimeMilliseconds, 0);
    pref_service_->CommitPendingWrite();
  }

  if (!last_active_check_.is_null() && last_state_active_) {
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity. This is the same strategy used to enterprise users.
    base::TimeDelta active_seconds = now - last_active_check_;
    if (active_seconds < base::TimeDelta::FromSeconds(0) ||
        active_seconds >= (2 * kUpdateChildActiveTimeInterval)) {
      activity_storage_->AddActivityPeriod(now - kUpdateChildActiveTimeInterval,
                                           now, now,
                                           GetUserForActivityReporting());
    } else {
      activity_storage_->AddActivityPeriod(last_active_check_, now, now,
                                           GetUserForActivityReporting());
    }

    activity_storage_->PruneActivityPeriods(
        now, max_stored_past_activity_interval_,
        max_stored_future_activity_interval_);
  }
  last_active_check_ = now;
}

void DeviceStatusCollector::SampleResourceUsage() {
  // Results must be written in the creation thread since that's where they
  // are read from in the Get*StatusAsync methods.
  DCHECK(thread_checker_.CalledOnValidThread());

  // If hardware reporting has been disabled, do nothing here.
  if (!report_hardware_status_)
    return;

  // Call out to the blocking pool to sample CPU stats.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      cpu_statistics_fetcher_,
      base::Bind(&DeviceStatusCollector::ReceiveCPUStatistics,
                 weak_factory_.GetWeakPtr(), base::Time::Now()));
}

void DeviceStatusCollector::ReceiveCPUStatistics(const base::Time& timestamp,
                                                 const std::string& stats) {
  int cpu_usage_percent = 0;
  if (stats.empty()) {
    DLOG(WARNING) << "Unable to read CPU statistics";
  } else {
    // Parse the data from /proc/stat, whose format is defined at
    // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.
    //
    // The CPU usage values in /proc/stat are measured in the imprecise unit
    // "jiffies", but we just care about the relative magnitude of "active" vs
    // "idle" so the exact value of a jiffy is irrelevant.
    //
    // An example value for this line:
    //
    // cpu 123 456 789 012 345 678
    //
    // We only care about the first four numbers: user_time, nice_time,
    // sys_time, and idle_time.
    uint64_t user = 0, nice = 0, system = 0, idle = 0;
    int vals = sscanf(stats.c_str(),
                      "cpu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &user,
                      &nice, &system, &idle);
    DCHECK_EQ(4, vals);

    // The values returned from /proc/stat are cumulative totals, so calculate
    // the difference between the last sample and this one.
    uint64_t active = user + nice + system;
    uint64_t total = active + idle;
    uint64_t last_total = last_cpu_active_ + last_cpu_idle_;
    DCHECK_GE(active, last_cpu_active_);
    DCHECK_GE(idle, last_cpu_idle_);
    DCHECK_GE(total, last_total);

    if ((total - last_total) > 0) {
      cpu_usage_percent =
          (100 * (active - last_cpu_active_)) / (total - last_total);
    }
    last_cpu_active_ = active;
    last_cpu_idle_ = idle;
  }

  DCHECK_LE(cpu_usage_percent, 100);
  ResourceUsage usage = {cpu_usage_percent,
                         base::SysInfo::AmountOfAvailablePhysicalMemory()};

  resource_usage_.push_back(usage);

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (resource_usage_.size() > kMaxResourceUsageSamples)
    resource_usage_.pop_front();

  std::unique_ptr<SampledData> sample = std::make_unique<SampledData>();
  sample->timestamp = base::Time::Now();

  if (report_power_status_) {
    runtime_probe::ProbeRequest request;
    request.add_categories(runtime_probe::ProbeRequest::battery);
    runtime_probe_->ProbeCategories(
        request, base::BindOnce(&DeviceStatusCollector::SampleProbeData,
                                weak_factory_.GetWeakPtr(), std::move(sample),
                                SamplingProbeResultCallback()));
  } else {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&InvokeCpuTempFetcher, cpu_temp_fetcher_),
        base::BindOnce(&DeviceStatusCollector::ReceiveCPUTemperature,
                       weak_factory_.GetWeakPtr(), std::move(sample),
                       SamplingCallback()));
  }
}

void DeviceStatusCollector::SampleProbeData(
    std::unique_ptr<SampledData> sample,
    SamplingProbeResultCallback callback,
    base::Optional<runtime_probe::ProbeResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result.has_value())
    return;
  if (result.value().error() != runtime_probe::RUNTIME_PROBE_ERROR_NOT_SET)
    return;

  if (result.value().battery_size() == 0)
    return;

  for (const auto& battery : result.value().battery()) {
    enterprise_management::BatterySample battery_sample;
    battery_sample.set_timestamp(sample->timestamp.ToJavaTime());
    // Convert uV to mV
    battery_sample.set_voltage(battery.values().voltage_now() / 1000);
    // Convert uAh to mAh
    battery_sample.set_remaining_capacity(battery.values().charge_now() / 1000);
    // Convert 0.1 Kelvin to Celsius
    battery_sample.set_temperature(
        (battery.values().temperature_smart() - 2731) / 10);
    sample->battery_samples[battery.name()] = battery_sample;
  }
  SamplingCallback completion_callback;
  if (!callback.is_null())
    completion_callback = base::BindOnce(std::move(callback), result);

  // PowerManagerClient::Observer::PowerChanged can be called as a result of
  // power_manager_->RequestStatusUpdate() as well as for other reasons,
  // so we store power_status_callback_ here instead of triggering
  // SampleDischargeRate from PowerChanged().
  DCHECK(power_status_callback_.is_null());  // Previous sampling is completed.

  power_status_callback_ = base::BindOnce(
      &DeviceStatusCollector::SampleDischargeRate, weak_factory_.GetWeakPtr(),
      std::move(sample), std::move(completion_callback));
  power_manager_->RequestStatusUpdate();
}

void DeviceStatusCollector::SampleDischargeRate(
    std::unique_ptr<SampledData> sample,
    SamplingCallback callback,
    const power_manager::PowerSupplyProperties& prop) {
  if (prop.has_battery_discharge_rate()) {
    int discharge_rate_mW =
        static_cast<int>(prop.battery_discharge_rate() * 1000);
    for (auto it = sample->battery_samples.begin();
         it != sample->battery_samples.end(); it++) {
      it->second.set_discharge_rate(discharge_rate_mW);
    }
  }

  if (prop.has_battery_percent() && prop.battery_percent() >= 0) {
    int percent = static_cast<int>(prop.battery_percent());
    for (auto it = sample->battery_samples.begin();
         it != sample->battery_samples.end(); it++) {
      it->second.set_charge_rate(percent);
    }
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InvokeCpuTempFetcher, cpu_temp_fetcher_),
      base::BindOnce(&DeviceStatusCollector::ReceiveCPUTemperature,
                     weak_factory_.GetWeakPtr(), std::move(sample),
                     std::move(callback)));
}

void DeviceStatusCollector::ReceiveCPUTemperature(
    std::unique_ptr<SampledData> sample,
    SamplingCallback callback,
    std::vector<em::CPUTempInfo> measurements) {
  auto timestamp = sample->timestamp.ToJavaTime();
  for (const auto& measurement : measurements) {
    sample->cpu_samples[measurement.cpu_label()] = measurement;
    sample->cpu_samples[measurement.cpu_label()].set_timestamp(timestamp);
  }
  AddDataSample(std::move(sample), std::move(callback));
}

void DeviceStatusCollector::AddDataSample(std::unique_ptr<SampledData> sample,
                                          SamplingCallback callback) {
  sampled_data_.push_back(std::move(sample));

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (sampled_data_.size() > kMaxResourceUsageSamples)
    sampled_data_.pop_front();
  // We have two code paths that end here. One is regular sampling, that does
  // not have final callback, and full report request, that would use callback
  // to receive ProbeResponse.
  if (!callback.is_null())
    std::move(callback).Run();
}

void DeviceStatusCollector::FetchProbeData(
    policy::DeviceStatusCollector::ProbeDataReceiver callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  runtime_probe::ProbeRequest request;
  if (report_power_status_)
    request.add_categories(runtime_probe::ProbeRequest::battery);
  if (report_storage_status_)
    request.add_categories(runtime_probe::ProbeRequest::storage);

  auto sample = std::make_unique<SampledData>();
  sample->timestamp = base::Time::Now();
  auto completion_callback =
      base::BindOnce(&DeviceStatusCollector::OnProbeDataFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  runtime_probe_->ProbeCategories(
      request, base::BindOnce(&DeviceStatusCollector::SampleProbeData,
                              weak_factory_.GetWeakPtr(), std::move(sample),
                              std::move(completion_callback)));
}

void DeviceStatusCollector::OnProbeDataFetched(
    policy::DeviceStatusCollector::ProbeDataReceiver callback,
    base::Optional<runtime_probe::ProbeResult> reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(reply, sampled_data_);
}

void DeviceStatusCollector::ReportingUsersChanged() {
  std::vector<std::string> reporting_users;
  for (auto& value :
       pref_service_->GetList(prefs::kReportingUsers)->GetList()) {
    if (value.is_string())
      reporting_users.push_back(value.GetString());
  }
  activity_storage_->FilterActivityPeriodsByUsers(reporting_users);
}

std::string DeviceStatusCollector::GetUserForActivityReporting() const {
  // Primary user is used as unique identifier of a single session, even for
  // multi-user sessions.
  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user || !primary_user->HasGaiaAccount())
    return std::string();

  // Report only affiliated users for enterprise reporting and signed-in user
  // for consumer reporting.
  std::string primary_user_email = primary_user->GetAccountId().GetUserEmail();
  if (is_enterprise_reporting_ &&
      !chromeos::ChromeUserManager::Get()->ShouldReportUser(
          primary_user_email)) {
    return std::string();
  }
  return primary_user_email;
}

bool DeviceStatusCollector::IncludeEmailsInActivityReports() const {
  // In enterprise reporting including users' emails depends on
  // kReportDeviceUsers preference. In consumer reporting only current user is
  // reported and email address is always included.
  return !is_enterprise_reporting_ || report_users_;
}

bool DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* status) {
  if (user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    UpdateChildUsageTime();
  }

  // If user reporting is off, data should be aggregated per day.
  // Signed-in user is reported in non-enterprise reporting.
  std::vector<ActivityStorage::ActivityPeriod> activity_times =
      activity_storage_->GetFilteredActivityPeriods(
          !IncludeEmailsInActivityReports());

  bool anything_reported = false;
  for (const auto& activity_period : activity_times) {
    // This is correct even when there are leap seconds, because when a leap
    // second occurs, two consecutive seconds have the same timestamp.
    int64_t end_timestamp =
        activity_period.start_timestamp + Time::kMillisecondsPerDay;

    em::ActiveTimePeriod* active_period = status->add_active_periods();
    em::TimePeriod* period = active_period->mutable_time_period();
    period->set_start_timestamp(activity_period.start_timestamp);
    period->set_end_timestamp(end_timestamp);
    active_period->set_active_duration(activity_period.activity_milliseconds);
    // Report user email only if users reporting is turned on.
    if (!activity_period.user_email.empty())
      active_period->set_user_email(activity_period.user_email);
    if (activity_period.start_timestamp >= last_reported_day_) {
      last_reported_day_ = activity_period.start_timestamp;
      duration_for_last_reported_day_ = activity_period.activity_milliseconds;
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* status) {
  status->set_os_version(os_version_);
  if (!is_enterprise_reporting_)
    return true;

  // Enterprise-only version reporting below.
  status->set_browser_version(version_info::GetVersionNumber());
  status->set_channel(ConvertToProtoChannel(chrome::GetChannel()));
  status->set_firmware_version(firmware_version_);

  em::TpmVersionInfo* const tpm_version_info =
      status->mutable_tpm_version_info();
  tpm_version_info->set_family(tpm_version_info_.family);
  tpm_version_info->set_spec_level(tpm_version_info_.spec_level);
  tpm_version_info->set_manufacturer(tpm_version_info_.manufacturer);
  tpm_version_info->set_tpm_model(tpm_version_info_.tpm_model);
  tpm_version_info->set_firmware_version(tpm_version_info_.firmware_version);
  tpm_version_info->set_vendor_specific(tpm_version_info_.vendor_specific);

  return true;
}

bool DeviceStatusCollector::GetBootMode(em::DeviceStatusReportRequest* status) {
  std::string dev_switch_mode;
  bool anything_reported = false;
  if (statistics_provider_->GetMachineStatistic(
          chromeos::system::kDevSwitchBootKey, &dev_switch_mode)) {
    if (dev_switch_mode == chromeos::system::kDevSwitchBootValueDev)
      status->set_boot_mode("Dev");
    else if (dev_switch_mode == chromeos::system::kDevSwitchBootValueVerified)
      status->set_boot_mode("Verified");
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetWriteProtectSwitch(
    em::DeviceStatusReportRequest* status) {
  std::string firmware_write_protect;
  if (!statistics_provider_->GetMachineStatistic(
          chromeos::system::kFirmwareWriteProtectBootKey,
          &firmware_write_protect)) {
    return false;
  }

  if (firmware_write_protect ==
      chromeos::system::kFirmwareWriteProtectBootValueOff) {
    status->set_write_protect_switch(false);
  } else if (firmware_write_protect ==
             chromeos::system::kFirmwareWriteProtectBootValueOn) {
    status->set_write_protect_switch(true);
  } else {
    return false;
  }
  return true;
}

bool DeviceStatusCollector::GetNetworkInterfaces(
    em::DeviceStatusReportRequest* status) {
  // Maps shill device type strings to proto enum constants.
  static const struct {
    const char* type_string;
    em::NetworkInterface::NetworkDeviceType type_constant;
  } kDeviceTypeMap[] = {
    { shill::kTypeEthernet,  em::NetworkInterface::TYPE_ETHERNET,  },
    { shill::kTypeWifi,      em::NetworkInterface::TYPE_WIFI,      },
    { shill::kTypeWimax,     em::NetworkInterface::TYPE_WIMAX,     },
    { shill::kTypeBluetooth, em::NetworkInterface::TYPE_BLUETOOTH, },
    { shill::kTypeCellular,  em::NetworkInterface::TYPE_CELLULAR,  },
  };

  // Maps shill device connection status to proto enum constants.
  static const struct {
    const char* state_string;
    em::NetworkState::ConnectionState state_constant;
  } kConnectionStateMap[] = {
      {shill::kStateIdle, em::NetworkState::IDLE},
      {shill::kStateCarrier, em::NetworkState::CARRIER},
      {shill::kStateAssociation, em::NetworkState::ASSOCIATION},
      {shill::kStateConfiguration, em::NetworkState::CONFIGURATION},
      {shill::kStateReady, em::NetworkState::READY},
      {shill::kStatePortal, em::NetworkState::PORTAL},
      {shill::kStateOffline, em::NetworkState::OFFLINE},
      {shill::kStateOnline, em::NetworkState::ONLINE},
      {shill::kStateDisconnect, em::NetworkState::DISCONNECT},
      {shill::kStateFailure, em::NetworkState::FAILURE},
      {shill::kStateActivationFailure, em::NetworkState::ACTIVATION_FAILURE},
  };

  chromeos::NetworkStateHandler::DeviceStateList device_list;
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetDeviceList(&device_list);

  bool anything_reported = false;
  chromeos::NetworkStateHandler::DeviceStateList::const_iterator device;
  for (device = device_list.begin(); device != device_list.end(); ++device) {
    // Determine the type enum constant for |device|.
    size_t type_idx = 0;
    for (; type_idx < base::size(kDeviceTypeMap); ++type_idx) {
      if ((*device)->type() == kDeviceTypeMap[type_idx].type_string)
        break;
    }

    // If the type isn't in |kDeviceTypeMap|, the interface is not relevant for
    // reporting. This filters out VPN devices.
    if (type_idx >= base::size(kDeviceTypeMap))
      continue;

    em::NetworkInterface* interface = status->add_network_interfaces();
    interface->set_type(kDeviceTypeMap[type_idx].type_constant);
    if (!(*device)->mac_address().empty())
      interface->set_mac_address((*device)->mac_address());
    if (!(*device)->meid().empty())
      interface->set_meid((*device)->meid());
    if (!(*device)->imei().empty())
      interface->set_imei((*device)->imei());
    if (!(*device)->path().empty())
      interface->set_device_path((*device)->path());
    anything_reported = true;
  }

  // Don't write any network state if we aren't in a kiosk or public session.
  if (!GetAutoLaunchedKioskSessionInfo() &&
      !user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
    return anything_reported;
  }

  // Walk the various networks and store their state in the status report.
  chromeos::NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(
      chromeos::NetworkTypePattern::Default(),
      true,   // configured_only
      false,  // visible_only
      0,      // no limit to number of results
      &state_list);

  for (const chromeos::NetworkState* state : state_list) {
    // Determine the connection state and signal strength for |state|.
    em::NetworkState::ConnectionState connection_state_enum =
        em::NetworkState::UNKNOWN;
    const std::string connection_state_string(state->connection_state());
    for (size_t i = 0; i < base::size(kConnectionStateMap); ++i) {
      if (connection_state_string == kConnectionStateMap[i].state_string) {
        connection_state_enum = kConnectionStateMap[i].state_constant;
        break;
      }
    }

    // Copy fields from NetworkState into the status report.
    em::NetworkState* proto_state = status->add_network_states();
    proto_state->set_connection_state(connection_state_enum);
    anything_reported = true;

    // Report signal strength for wifi connections.
    if (state->type() == shill::kTypeWifi) {
      // If shill has provided a signal strength, convert it to dBm and store it
      // in the status report. A signal_strength() of 0 connotes "no signal"
      // rather than "really weak signal", so we only report signal strength if
      // it is non-zero.
      if (state->signal_strength()) {
        proto_state->set_signal_strength(
            ConvertWifiSignalStrength(state->signal_strength()));
      }
    }

    if (!state->device_path().empty())
      proto_state->set_device_path(state->device_path());

    std::string ip_address = state->GetIpAddress();
    if (!ip_address.empty())
      proto_state->set_ip_address(ip_address);

    std::string gateway = state->GetGateway();
    if (!gateway.empty())
      proto_state->set_gateway(gateway);
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetUsers(em::DeviceStatusReportRequest* status) {
  const user_manager::UserList& users =
      chromeos::ChromeUserManager::Get()->GetUsers();

  bool anything_reported = false;
  for (auto* user : users) {
    // Only users with gaia accounts (regular) are reported.
    if (!user->HasGaiaAccount())
      continue;

    em::DeviceUser* device_user = status->add_users();
    if (chromeos::ChromeUserManager::Get()->ShouldReportUser(
            user->GetAccountId().GetUserEmail())) {
      device_user->set_type(em::DeviceUser::USER_TYPE_MANAGED);
      device_user->set_email(user->GetAccountId().GetUserEmail());
    } else {
      device_user->set_type(em::DeviceUser::USER_TYPE_UNMANAGED);
      // Do not report the email address of unmanaged users.
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetHardwareStatus(
    em::DeviceStatusReportRequest* status,
    scoped_refptr<GetStatusState> state) {
  // Sample disk volume info in a background thread.
  state->SampleVolumeInfo(volume_info_fetcher_);

  // Add CPU utilization and free RAM. Note that these stats are sampled in
  // regular intervals. Unlike CPU temp and volume info these are not one-time
  // sampled values, hence the difference in logic.
  status->set_system_ram_total(base::SysInfo::AmountOfPhysicalMemory());
  status->clear_system_ram_free_samples();
  status->clear_cpu_utilization_pct_samples();
  for (const ResourceUsage& usage : resource_usage_) {
    status->add_cpu_utilization_pct_samples(usage.cpu_usage_percent);
    status->add_system_ram_free_samples(usage.bytes_of_ram_free);
  }

  // Get the current device sound volume level.
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  status->set_sound_volume(audio_handler->GetOutputVolumePercent());

  // Fetch TPM status information on a background thread.
  state->FetchTpmStatus(tpm_status_fetcher_);

  // clear
  status->clear_cpu_temp_infos();

  if (report_power_status_ || report_storage_status_) {
    state->FetchEMMCLifeTime(emmc_lifetime_fetcher_);
    state->FetchProbeData(probe_data_fetcher_);
  } else {
    // Sample CPU temperature in a background thread.
    state->SampleCPUTempInfo(cpu_temp_fetcher_);
  }
  return true;
}

bool DeviceStatusCollector::GetOsUpdateStatus(
    em::DeviceStatusReportRequest* status) {
  const base::Version platform_version(GetPlatformVersion());
  if (!platform_version.IsValid())
    return false;

  const std::string required_platform_version_string =
      chromeos::KioskAppManager::Get()
          ->GetAutoLaunchAppRequiredPlatformVersion();
  if (required_platform_version_string.empty())
    return false;

  const base::Version required_platfrom_version(
      required_platform_version_string);

  em::OsUpdateStatus* os_update_status = status->mutable_os_update_status();
  os_update_status->set_new_required_platform_version(
      required_platfrom_version.GetString());

  if (platform_version == required_platfrom_version) {
    os_update_status->set_update_status(em::OsUpdateStatus::OS_UP_TO_DATE);
    return true;
  }

  const chromeos::UpdateEngineClient::Status update_engine_status =
      chromeos::DBusThreadManager::Get()
          ->GetUpdateEngineClient()
          ->GetLastStatus();
  if (update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS);
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else if (update_engine_status.status ==
             chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT);
    // Note the new_version could be a dummy "0.0.0.0" for some edge cases,
    // e.g. update engine is somehow restarted without a reboot.
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED);
  }

  return true;
}

bool DeviceStatusCollector::GetRunningKioskApp(
    em::DeviceStatusReportRequest* status) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  // Only generate running kiosk app reports if we are in an auto-launched kiosk
  // session.
  if (!account)
    return false;

  em::AppStatus* running_kiosk_app = status->mutable_running_kiosk_app();
  if (account->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    running_kiosk_app->set_app_id(account->kiosk_app_id);

    const std::string app_version = GetAppVersion(account->kiosk_app_id);
    if (app_version.empty()) {
      DLOG(ERROR) << "Unable to get version for extension: "
                  << account->kiosk_app_id;
    } else {
      running_kiosk_app->set_extension_version(app_version);
    }

    chromeos::KioskAppManager::App app_info;
    if (chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                                 &app_info)) {
      running_kiosk_app->set_required_platform_version(
          app_info.required_platform_version);
    }
  } else if (account->type == policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    // Use package name as app ID for ARC Kiosks.
    running_kiosk_app->set_app_id(account->arc_kiosk_app_info.package_name());
  } else {
    NOTREACHED();
  }
  return true;
}

void DeviceStatusCollector::GetStatusAsync(
    const StatusCollectorCallback& response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());
  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<GetStatusState> state(
      new GetStatusState(task_runner_, response));
  // Gather device status (might queue some async queries)
  GetDeviceStatus(state);

  // Gather session status (might queue some async queries)
  GetSessionStatus(state);

  // If there are no outstanding async queries, e.g. from GetHardwareStatus(),
  // the destructor of |state| calls |response|. If there are async queries, the
  // queries hold references to |state|, so that |state| is only destroyed when
  // the last async query has finished.
}

void DeviceStatusCollector::GetDeviceStatus(
    scoped_refptr<GetStatusState> state) {
  em::DeviceStatusReportRequest* status = state->device_status();
  bool anything_reported = false;

  if (report_activity_times_)
    anything_reported |= GetActivityTimes(status);

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  if (report_boot_mode_)
    anything_reported |= GetBootMode(status);

  if (report_network_interfaces_)
    anything_reported |= GetNetworkInterfaces(status);

  if (report_users_)
    anything_reported |= GetUsers(status);

  if (report_hardware_status_) {
    anything_reported |= GetHardwareStatus(status, state);
    anything_reported |= GetWriteProtectSwitch(status);
  }

  if (report_os_update_status_)
    anything_reported |= GetOsUpdateStatus(status);

  if (report_running_kiosk_app_)
    anything_reported |= GetRunningKioskApp(status);

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->ResetDeviceStatus();
}

std::string DeviceStatusCollector::GetDMTokenForProfile(Profile* profile) {
  CloudPolicyManager* user_cloud_policy_manager =
      UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          profile);
  if (!user_cloud_policy_manager) {
    NOTREACHED();
    return std::string();
  }

  auto* cloud_policy_client = user_cloud_policy_manager->core()->client();
  std::string dm_token = cloud_policy_client->dm_token();

  return dm_token;
}

bool DeviceStatusCollector::GetSessionStatusForUser(
    scoped_refptr<GetStatusState> state,
    em::SessionStatusReportRequest* status,
    const user_manager::User* user) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;

  bool anything_reported_user = false;

  const bool report_android_status =
      profile->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);
  if (report_android_status)
    anything_reported_user |= GetAndroidStatus(status, state);

  const bool report_crostini_usage = profile->GetPrefs()->GetBoolean(
      crostini::prefs::kReportCrostiniUsageEnabled);
  if (report_crostini_usage)
    anything_reported_user |= GetCrostiniUsage(status, profile);

  if (anything_reported_user && !user->IsDeviceLocalAccount())
    status->set_user_dm_token(GetDMTokenForProfile(profile));

  // Time zone is not reported in enterprise reports.
  if (!is_enterprise_reporting_) {
    const std::string current_timezone =
        base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                              ->GetCurrentTimezoneID());
    status->set_time_zone(current_timezone);
    anything_reported_user = true;
  }

  return anything_reported_user;
}

void DeviceStatusCollector::GetSessionStatus(
    scoped_refptr<GetStatusState> state) {
  em::SessionStatusReportRequest* status = state->session_status();
  bool anything_reported = false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();

  if (report_kiosk_session_status_)
    anything_reported |= GetKioskSessionStatus(status);

  // Only report affiliated users' data in enterprise reporting and registered
  // user data in consumer reporting. Note that device-local accounts are also
  // affiliated. Currently we only report for the primary user.
  if (primary_user &&
      (!is_enterprise_reporting_ || primary_user->IsAffiliated())) {
    anything_reported |= GetSessionStatusForUser(state, status, primary_user);
  }

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->ResetSessionStatus();
}

bool DeviceStatusCollector::GetKioskSessionStatus(
    em::SessionStatusReportRequest* status) {
  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  if (!account)
    return false;

  // Get the account ID associated with this user.
  status->set_device_local_account_id(account->account_id);
  em::AppStatus* app_status = status->add_installed_apps();
  if (account->type == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    app_status->set_app_id(account->kiosk_app_id);

    // Look up the app and get the version.
    const std::string app_version = GetAppVersion(account->kiosk_app_id);
    if (app_version.empty()) {
      DLOG(ERROR) << "Unable to get version for extension: "
                  << account->kiosk_app_id;
    } else {
      app_status->set_extension_version(app_version);
    }
  } else if (account->type == policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP) {
    // Use package name as app ID for ARC Kiosks.
    app_status->set_app_id(account->arc_kiosk_app_info.package_name());
  } else {
    NOTREACHED();
  }

  return true;
}

bool DeviceStatusCollector::GetAndroidStatus(
    em::SessionStatusReportRequest* status,
    const scoped_refptr<GetStatusState>& state) {
  return state->FetchAndroidStatus(android_status_fetcher_);
}

bool DeviceStatusCollector::GetCrostiniUsage(
    em::SessionStatusReportRequest* status,
    Profile* profile) {
  if (!profile->GetPrefs()->HasPrefPath(
          crostini::prefs::kCrostiniLastLaunchTimeWindowStart)) {
    return false;
  }

  em::CrostiniStatus* const crostini_status = status->mutable_crostini_status();
  const int64_t last_launch_time_window_start = profile->GetPrefs()->GetInt64(
      crostini::prefs::kCrostiniLastLaunchTimeWindowStart);
  const std::string& termina_version = profile->GetPrefs()->GetString(
      crostini::prefs::kCrostiniLastLaunchVersion);
  crostini_status->set_last_launch_time_window_start_timestamp(
      last_launch_time_window_start);
  crostini_status->set_last_launch_vm_image_version(termina_version);

  return true;
}

std::string DeviceStatusCollector::GetAppVersion(
    const std::string& kiosk_app_id) {
  Profile* const profile = chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
  const extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* const extension = registry->GetExtensionById(
      kiosk_app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return std::string();
  return extension->VersionString();
}

// TODO(crbug.com/827386): move public API methods above private ones after
// common methods are extracted.
void DeviceStatusCollector::OnSubmittedSuccessfully() {
  activity_storage_->TrimActivityPeriods(last_reported_day_,
                                         duration_for_last_reported_day_,
                                         std::numeric_limits<int64_t>::max());
}

bool DeviceStatusCollector::ShouldReportActivityTimes() const {
  return report_activity_times_;
}
bool DeviceStatusCollector::ShouldReportNetworkInterfaces() const {
  return report_network_interfaces_;
}
bool DeviceStatusCollector::ShouldReportUsers() const {
  return report_users_;
}
bool DeviceStatusCollector::ShouldReportHardwareStatus() const {
  return report_hardware_status_;
}

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(const std::string& version) {
  firmware_version_ = version;
}

void DeviceStatusCollector::OnTpmVersion(
    const chromeos::CryptohomeClient::TpmVersionInfo& tpm_version_info) {
  tpm_version_info_ = tpm_version_info;
}

}  // namespace policy
