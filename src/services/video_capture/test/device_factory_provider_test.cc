// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/device_factory_provider_test.h"

#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/video_capture/public/mojom/constants.mojom.h"

namespace video_capture {

DeviceFactoryProviderTest::DeviceFactoryProviderTest()
    : service_manager::test::ServiceTest("video_capture_unittests") {}

DeviceFactoryProviderTest::~DeviceFactoryProviderTest() = default;

void DeviceFactoryProviderTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeJpegDecodeAccelerator);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream, "device-count=3");

  service_manager::test::ServiceTest::SetUp();

  connector()->BindInterface(mojom::kServiceName, &factory_provider_);
  // Note, that we explicitly do *not* call
  // |factory_provider_->InjectGpuDependencies()| here. Test case
  // |FakeMjpegVideoCaptureDeviceTest.
  //  CanDecodeMjpegWithoutInjectedGpuDependencies| depends on this assumption.
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory_));
}

}  // namespace video_capture
