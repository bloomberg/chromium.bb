// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_
#define REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace remoting {

// Class that provides device ID to be used to sign in for FTL.
// TODO(yuweih): Add unittest
class FtlDeviceIdProvider final {
 public:
  class TokenStorage {
   public:
    TokenStorage() = default;
    virtual ~TokenStorage() = default;

    virtual std::string FetchDeviceId() = 0;
    virtual bool StoreDeviceId(const std::string& device_id) = 0;
  };

  explicit FtlDeviceIdProvider(TokenStorage* token_storage);
  ~FtlDeviceIdProvider();

  // Gets a device ID to use for signing into FTL. This could either be a
  // previously stored value or a newly generated value (which has been already
  // stored into |token_storage_|).
  std::string GetDeviceId();

 private:
  TokenStorage* token_storage_;
  DISALLOW_COPY_AND_ASSIGN(FtlDeviceIdProvider);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_
