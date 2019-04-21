// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"

namespace cryptauthv2 {
class ClientAppMetadata;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Provides the cryptauthv2::ClientAppMetadata object associated with the
// current device. cryptauthv2::ClientAppMetadata describes properties of this
// Chromebook and is not expected to change except when the OS version is
// updated.
class ClientAppMetadataProvider {
 public:
  ClientAppMetadataProvider() = default;
  virtual ~ClientAppMetadataProvider() = default;

  using GetMetadataCallback = base::OnceCallback<void(
      const base::Optional<cryptauthv2::ClientAppMetadata>&)>;

  // Fetches the ClientAppMetadata for the current device; if the operation
  // fails, null is passed to the callback.
  virtual void GetClientAppMetadata(const std::string& gcm_registration_id,
                                    GetMetadataCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientAppMetadataProvider);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_CLIENT_APP_METADATA_PROVIDER_H_
