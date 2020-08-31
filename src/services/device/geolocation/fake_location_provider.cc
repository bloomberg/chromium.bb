// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a fake location provider and the factory functions for
// various ways of creating it.
// TODO(lethalantidote): Convert location_arbitrator_impl to use actual mock
// instead of FakeLocationProvider.

#include "services/device/geolocation/fake_location_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

FakeLocationProvider::FakeLocationProvider()
    : provider_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeLocationProvider::~FakeLocationProvider() = default;

void FakeLocationProvider::HandlePositionChanged(
    const mojom::Geoposition& position) {
  if (provider_task_runner_->BelongsToCurrentThread()) {
    // The location arbitrator unit tests rely on this method running
    // synchronously.
    position_ = position;
    if (!callback_.is_null())
      callback_.Run(this, position_);
  } else {
    provider_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeLocationProvider::HandlePositionChanged,
                                  base::Unretained(this), position));
  }
}

void FakeLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;
}

void FakeLocationProvider::StartProvider(bool high_accuracy) {
  state_ = high_accuracy ? HIGH_ACCURACY : LOW_ACCURACY;
}

void FakeLocationProvider::StopProvider() {
  state_ = STOPPED;
}

const mojom::Geoposition& FakeLocationProvider::GetPosition() {
  return position_;
}

void FakeLocationProvider::OnPermissionGranted() {
  is_permission_granted_ = true;
}

}  // namespace device
