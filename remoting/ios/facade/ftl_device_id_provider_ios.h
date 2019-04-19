// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_FTL_DEVICE_ID_PROVIDER_IOS_H_
#define REMOTING_IOS_FACADE_FTL_DEVICE_ID_PROVIDER_IOS_H_

#include <string>

#include "base/macros.h"
#include "remoting/signaling/ftl_device_id_provider.h"

namespace remoting {

class FtlDeviceIdProviderIos final : public FtlDeviceIdProvider {
 public:
  FtlDeviceIdProviderIos();
  ~FtlDeviceIdProviderIos() override;

  ftl::DeviceId GetDeviceId() override;

 private:
  ftl::DeviceId device_id_;

  DISALLOW_COPY_AND_ASSIGN(FtlDeviceIdProviderIos);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_FTL_DEVICE_ID_PROVIDER_IOS_H_
