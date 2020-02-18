// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_pip_instance.h"

#include <utility>

namespace arc {

FakePipInstance::FakePipInstance() = default;

FakePipInstance::~FakePipInstance() = default;

void FakePipInstance::Init(mojom::PipHostPtr host_ptr, InitCallback callback) {
  host_ptr_ = std::move(host_ptr);
  std::move(callback).Run();
}

void FakePipInstance::ClosePip() {
  num_closed_++;
}

void FakePipInstance::SetPipSuppressionStatus(bool suppressed) {
  suppressed_ = suppressed;
}

}  // namespace arc
