// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource(
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PowerMonitorBroadcastSource(std::make_unique<Client>(),
                                  connector,
                                  task_runner) {}

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource(
    std::unique_ptr<Client> client,
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : client_(std::move(client)), task_runner_(task_runner) {
  if (connector) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerMonitorBroadcastSource::Client::Init,
                                  base::Unretained(client_.get()),
                                  base::Passed(connector->Clone())));
  }
}

PowerMonitorBroadcastSource::~PowerMonitorBroadcastSource() {
  task_runner_->DeleteSoon(FROM_HERE, client_.release());
}

bool PowerMonitorBroadcastSource::IsOnBatteryPowerImpl() {
  return client_->last_reported_battery_power_state();
}

PowerMonitorBroadcastSource::Client::Client()
    : last_reported_battery_power_state_(false), binding_(this) {}

PowerMonitorBroadcastSource::Client::~Client() {}

void PowerMonitorBroadcastSource::Client::Init(
    std::unique_ptr<service_manager::Connector> connector) {
  connector_ = std::move(connector);
  device::mojom::PowerMonitorPtr power_monitor;
  connector_->BindInterface(device::mojom::kServiceName,
                            mojo::MakeRequest(&power_monitor));
  device::mojom::PowerMonitorClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  power_monitor->AddClient(std::move(client));
}

bool PowerMonitorBroadcastSource::Client::last_reported_battery_power_state() {
  return base::subtle::NoBarrier_Load(&last_reported_battery_power_state_) != 0;
}

void PowerMonitorBroadcastSource::Client::PowerStateChange(
    bool on_battery_power) {
  base::subtle::NoBarrier_Store(&last_reported_battery_power_state_,
                                on_battery_power);
  ProcessPowerEvent(PowerMonitorSource::POWER_STATE_EVENT);
}

void PowerMonitorBroadcastSource::Client::Suspend() {
  ProcessPowerEvent(PowerMonitorSource::SUSPEND_EVENT);
}

void PowerMonitorBroadcastSource::Client::Resume() {
  ProcessPowerEvent(PowerMonitorSource::RESUME_EVENT);
}

}  // namespace device
