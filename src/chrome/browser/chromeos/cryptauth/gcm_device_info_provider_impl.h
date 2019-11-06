// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/public/cpp/gcm_device_info_provider.h"

namespace chromeos {

// Concrete GcmDeviceInfoProvider implementation.
class GcmDeviceInfoProviderImpl : public device_sync::GcmDeviceInfoProvider {
 public:
  static const GcmDeviceInfoProviderImpl* GetInstance();

  // device_sync::GcmDeviceInfoProvider:
  const cryptauth::GcmDeviceInfo& GetGcmDeviceInfo() const override;

 private:
  friend class base::NoDestructor<GcmDeviceInfoProviderImpl>;

  GcmDeviceInfoProviderImpl();

  DISALLOW_COPY_AND_ASSIGN(GcmDeviceInfoProviderImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_
