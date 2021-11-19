// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_monitor.h"
#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"

// MediaNotificationDeviceMonitor
MediaNotificationDeviceMonitor::~MediaNotificationDeviceMonitor() = default;

std::unique_ptr<MediaNotificationDeviceMonitor>
MediaNotificationDeviceMonitor::Create(
    MediaNotificationDeviceProvider* device_provider) {
// The device monitor implementation on linux does not reliably detect
// connection changes for some devices. In this case we fall back to polling the
// device provider. See crbug.com/1112480 for more information.
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_UDEV)
  return std::make_unique<PollingDeviceMonitorImpl>(device_provider);
#else
  return std::make_unique<SystemMonitorDeviceMonitorImpl>();
#endif
}

void MediaNotificationDeviceMonitor::AddDevicesChangedObserver(
    DevicesChangedObserver* obs) {
  observers_.AddObserver(obs);
}

void MediaNotificationDeviceMonitor::RemoveDevicesChangedObserver(
    DevicesChangedObserver* obs) {
  observers_.RemoveObserver(obs);
}

MediaNotificationDeviceMonitor::MediaNotificationDeviceMonitor() = default;

#if !((defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_UDEV))
// SystemMonitorDeviceMonitorImpl
SystemMonitorDeviceMonitorImpl::SystemMonitorDeviceMonitorImpl() {
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
}

SystemMonitorDeviceMonitorImpl::~SystemMonitorDeviceMonitorImpl() {
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void SystemMonitorDeviceMonitorImpl::StartMonitoring() {
  is_monitoring_ = true;
}

void SystemMonitorDeviceMonitorImpl::StopMonitoring() {
  is_monitoring_ = false;
}

void SystemMonitorDeviceMonitorImpl::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (!is_monitoring_)
    return;
  // TODO(noahrose): Get an issue number for this TODO. Only notify observers in
  // changes of audio output devices as opposed to audio devices in general.
  if (device_type != base::SystemMonitor::DEVTYPE_AUDIO)
    return;

  for (auto& observer : observers_) {
    observer.OnDevicesChanged();
  }
}
#else
namespace {
constexpr int kPollingIntervalSeconds = 10;
}  // anonymous namespace

// PollingDeviceMonitorImpl
PollingDeviceMonitorImpl::PollingDeviceMonitorImpl(
    MediaNotificationDeviceProvider* device_provider)
    : device_provider_(device_provider),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void PollingDeviceMonitorImpl::StartMonitoring() {
  if (is_monitoring_)
    return;

  is_monitoring_ = true;
  if (!is_task_posted_) {
    is_task_posted_ = true;
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PollingDeviceMonitorImpl::PollDeviceProvider,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(kPollingIntervalSeconds));
  }
}

void PollingDeviceMonitorImpl::StopMonitoring() {
  is_monitoring_ = false;
}

// static
int PollingDeviceMonitorImpl::get_polling_interval_for_testing() {
  return kPollingIntervalSeconds;
}

PollingDeviceMonitorImpl::~PollingDeviceMonitorImpl() = default;

void PollingDeviceMonitorImpl::PollDeviceProvider() {
  is_task_posted_ = false;
  if (!is_monitoring_)
    return;
  device_provider_->GetOutputDeviceDescriptions(
      base::BindOnce(&PollingDeviceMonitorImpl::OnDeviceDescriptionsRecieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PollingDeviceMonitorImpl::OnDeviceDescriptionsRecieved(
    media::AudioDeviceDescriptions descriptions) {
  if (!is_monitoring_)
    return;

  if (!std::equal(descriptions.cbegin(), descriptions.cend(),
                  device_ids_.cbegin(), device_ids_.cend(),
                  [](const auto& description, const auto& id) {
                    return description.unique_id == id;
                  })) {
    device_ids_.clear();
    std::transform(descriptions.begin(), descriptions.end(),
                   std::back_inserter(device_ids_), [](auto& description) {
                     return std::move(description.unique_id);
                   });
    NotifyObservers();
  }

  is_task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PollingDeviceMonitorImpl::PollDeviceProvider,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kPollingIntervalSeconds));
}

void PollingDeviceMonitorImpl::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnDevicesChanged();
  }
}
#endif
