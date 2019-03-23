// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_device_id_provider.h"

#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace remoting {

namespace {

#if defined(OS_ANDROID)
constexpr char kDeviceIdPrefix[] = "crd-android-";
#elif defined(OS_IOS)
constexpr char kDeviceIdPrefix[] = "crd-ios-";
#elif defined(OS_WIN)
constexpr char kDeviceIdPrefix[] = "crd-win-host-";
#elif defined(OS_MACOSX)
constexpr char kDeviceIdPrefix[] = "crd-mac-host-";
#elif defined(OS_CHROMEOS)
constexpr char kDeviceIdPrefix[] = "crd-cros-host-";
#elif defined(OS_LINUX)
constexpr char kDeviceIdPrefix[] = "crd-linux-host-";
#else
constexpr char kDeviceIdPrefix[] = "crd-unknown-";
#endif

}  // namespace

FtlDeviceIdProvider::FtlDeviceIdProvider(TokenStorage* token_storage)
    : token_storage_(token_storage) {
  DCHECK(token_storage_);
}

FtlDeviceIdProvider::~FtlDeviceIdProvider() = default;

std::string FtlDeviceIdProvider::GetDeviceId() {
  std::string device_id = token_storage_->FetchDeviceId();
  if (device_id.empty()) {
    device_id = kDeviceIdPrefix + base::GenerateGUID();
    VLOG(0) << "Generated new device_id: " << device_id;
    token_storage_->StoreDeviceId(device_id);
  } else {
    VLOG(0) << "Using stored device_id: " << device_id;
  }
  return device_id;
}

}  // namespace remoting
