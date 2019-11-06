// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "chromeos/services/device_sync/public/cpp/cryptauth_device_id_provider.h"

class PrefRegistrySimple;

namespace cryptauth {

// Concrete CryptAuthDeviceIdProvider implementation which stores the device ID
// in the browser process' local state PrefStore.
class CryptAuthDeviceIdProviderImpl
    : public chromeos::device_sync::CryptAuthDeviceIdProvider {
 public:
  // Registers the prefs used by this class. |registry| must be associated
  // with browser local storage, not an individual profile.
  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  static const CryptAuthDeviceIdProviderImpl* GetInstance();

  // CryptAuthDeviceIdProvider:
  std::string GetDeviceId() const override;

 private:
  friend class base::NoDestructor<CryptAuthDeviceIdProviderImpl>;

  CryptAuthDeviceIdProviderImpl();

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceIdProviderImpl);
};

}  // namespace cryptauth

#endif  // CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_IMPL_H_
