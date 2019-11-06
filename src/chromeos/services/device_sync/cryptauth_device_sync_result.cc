// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"

namespace chromeos {

namespace device_sync {

CryptAuthDeviceSyncResult::CryptAuthDeviceSyncResult(
    ResultCode result_code,
    const base::Optional<cryptauthv2::ClientDirective>& client_directive)
    : result_code_(result_code), client_directive_(client_directive) {}

CryptAuthDeviceSyncResult::CryptAuthDeviceSyncResult(
    const CryptAuthDeviceSyncResult& other) = default;

CryptAuthDeviceSyncResult::~CryptAuthDeviceSyncResult() = default;

bool CryptAuthDeviceSyncResult::IsSuccess() const {
  return result_code_ == ResultCode::kSuccess;
}

bool CryptAuthDeviceSyncResult::operator==(
    const CryptAuthDeviceSyncResult& other) const {
  return result_code_ == other.result_code_ &&
         client_directive_.has_value() == other.client_directive_.has_value() &&
         (!client_directive_ ||
          client_directive_->SerializeAsString() ==
              other.client_directive_->SerializeAsString());
}

bool CryptAuthDeviceSyncResult::operator!=(
    const CryptAuthDeviceSyncResult& other) const {
  return !(*this == other);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthDeviceSyncResult::ResultCode result_code) {
  using ResultCode = CryptAuthDeviceSyncResult::ResultCode;

  switch (result_code) {
    case ResultCode::kSuccess:
      stream << "[Success]";
      break;
    case ResultCode::kError:
      stream << "[Error]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace chromeos
