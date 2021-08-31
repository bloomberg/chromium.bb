// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_util.h"

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/device_form_factor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/statistics_provider.h"
#endif

namespace syncer {

LocalDeviceNameInfo::LocalDeviceNameInfo() = default;
LocalDeviceNameInfo::LocalDeviceNameInfo(const LocalDeviceNameInfo& other) =
    default;
LocalDeviceNameInfo::~LocalDeviceNameInfo() = default;

namespace {

void OnLocalDeviceNameInfoReady(
    base::OnceCallback<void(LocalDeviceNameInfo)> callback,
    std::unique_ptr<LocalDeviceNameInfo> name_info) {
  std::move(callback).Run(std::move(*name_info));
}

void OnHardwareInfoReady(LocalDeviceNameInfo* name_info_ptr,
                         base::ScopedClosureRunner done_closure,
                         base::SysInfo::HardwareInfo hardware_info) {
  name_info_ptr->manufacturer_name = std::move(hardware_info.manufacturer);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For ChromeOS the returned model values are product code names like Eve. We
  // want to use generic names like Chromebook.
  name_info_ptr->model_name = GetChromeOSDeviceNameFromType();
#else
  name_info_ptr->model_name = std::move(hardware_info.model);
#endif
}

void OnPersonalizableDeviceNameReady(LocalDeviceNameInfo* name_info_ptr,
                                     base::ScopedClosureRunner done_closure,
                                     std::string personalizable_name) {
  name_info_ptr->personalizable_name = std::move(personalizable_name);
}

void OnMachineStatisticsLoaded(LocalDeviceNameInfo* name_info_ptr,
                               base::ScopedClosureRunner done_closure) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // |full_hardware_class| is set on Chrome OS devices if the user has UMA
  // enabled. Otherwise |full_hardware_class| is set to an empty string.
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      chromeos::system::kHardwareClassKey, &name_info_ptr->full_hardware_class);
#else
  name_info_ptr->full_hardware_class = "";
#endif
}

}  // namespace

// Declared here but defined in platform-specific files.
std::string GetPersonalizableDeviceNameInternal();

sync_pb::SyncEnums::DeviceType GetLocalDeviceType() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
#elif defined(OS_ANDROID) || defined(OS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? sync_pb::SyncEnums_DeviceType_TYPE_TABLET
             : sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
#elif defined(OS_MAC)
  return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
#else
  return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
#endif
}

std::string GetPersonalizableDeviceNameBlocking() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string device_name = GetPersonalizableDeviceNameInternal();

  if (device_name == "Unknown" || device_name.empty()) {
    device_name = base::SysInfo::OperatingSystemName();
  }

  DCHECK(base::IsStringUTF8(device_name));
  return device_name;
}

void GetLocalDeviceNameInfo(
    base::OnceCallback<void(LocalDeviceNameInfo)> callback) {
  auto name_info = std::make_unique<LocalDeviceNameInfo>();
  LocalDeviceNameInfo* name_info_ptr = name_info.get();

  auto done_closure = base::BarrierClosure(
      /*num_closures=*/3,
      base::BindOnce(&OnLocalDeviceNameInfoReady, std::move(callback),
                     std::move(name_info)));

  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&OnHardwareInfoReady, name_info_ptr,
                     base::ScopedClosureRunner(done_closure)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bind hwclass once the statistics are available on ChromeOS devices.
  chromeos::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(
          base::BindOnce(&OnMachineStatisticsLoaded, name_info_ptr,
                         base::ScopedClosureRunner(done_closure)));
#else
  OnMachineStatisticsLoaded(name_info_ptr,
                            base::ScopedClosureRunner(done_closure));
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetPersonalizableDeviceNameBlocking),
      base::BindOnce(&OnPersonalizableDeviceNameReady, name_info_ptr,
                     base::ScopedClosureRunner(done_closure)));
}

}  // namespace syncer
