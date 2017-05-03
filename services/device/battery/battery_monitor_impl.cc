// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_monitor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// static
void BatteryMonitorImpl::Create(mojom::BatteryMonitorRequest request) {
  auto* impl = new BatteryMonitorImpl;
  auto binding =
      mojo::MakeStrongBinding(base::WrapUnique(impl), std::move(request));
  impl->binding_ = binding;
}

BatteryMonitorImpl::BatteryMonitorImpl() : status_to_report_(false) {
  // NOTE: DidChange may be called before AddCallback returns. This is done to
  // report current status.
  subscription_ = BatteryStatusService::GetInstance()->AddCallback(
      base::Bind(&BatteryMonitorImpl::DidChange, base::Unretained(this)));
}

BatteryMonitorImpl::~BatteryMonitorImpl() {}

void BatteryMonitorImpl::QueryNextStatus(
    const QueryNextStatusCallback& callback) {
  if (!callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextStatus!";
    binding_->Close();
    return;
  }
  callback_ = callback;

  if (status_to_report_)
    ReportStatus();
}

void BatteryMonitorImpl::RegisterSubscription() {}

void BatteryMonitorImpl::DidChange(const mojom::BatteryStatus& battery_status) {
  status_ = battery_status;
  status_to_report_ = true;

  if (!callback_.is_null())
    ReportStatus();
}

void BatteryMonitorImpl::ReportStatus() {
  callback_.Run(status_.Clone());
  callback_.Reset();

  status_to_report_ = false;
}

}  // namespace device
