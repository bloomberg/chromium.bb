// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_MANIFEST_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace chromeos {
namespace device_sync {

const service_manager::Manifest& GetManifest();

}  // namespace device_sync
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_MANIFEST_H_
