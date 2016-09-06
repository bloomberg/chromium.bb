// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/fake_device_test.h"

#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceTest::FakeDeviceTest() : FakeDeviceDescriptorTest() {}

FakeDeviceTest::~FakeDeviceTest() = default;

void FakeDeviceTest::SetUp() {
  FakeDeviceDescriptorTest::SetUp();

  factory_->CreateDeviceProxy(
      std::move(fake_device_descriptor_), mojo::GetProxy(&fake_device_proxy_),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
}

}  // namespace video_capture
