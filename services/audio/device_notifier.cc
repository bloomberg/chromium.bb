// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_notifier.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace audio {

DeviceNotifier::DeviceNotifier()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

DeviceNotifier::~DeviceNotifier() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void DeviceNotifier::Bind(
    mojom::DeviceNotifierRequest request,
    std::unique_ptr<service_manager::ServiceContextRef> context_ref) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  bindings_.AddBinding(this, std::move(request), std::move(context_ref));
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
}

void DeviceNotifier::RegisterListener(mojom::DeviceListenerPtr listener) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  int listener_id = next_listener_id_++;
  listener.set_connection_error_handler(
      base::BindRepeating(&DeviceNotifier::RemoveListener,
                          weak_factory_.GetWeakPtr(), listener_id));
  listeners_[listener_id] = std::move(listener);
}

void DeviceNotifier::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type != base::SystemMonitor::DEVTYPE_AUDIO)
    return;

  task_runner_->PostTask(FROM_HERE,
                         base::BindRepeating(&DeviceNotifier::UpdateListeners,
                                             weak_factory_.GetWeakPtr()));
}

void DeviceNotifier::UpdateListeners() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (const auto& listener : listeners_)
    listener.second->DevicesChanged();
}

void DeviceNotifier::RemoveListener(int listener_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  listeners_.erase(listener_id);
}

}  // namespace audio
