// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_UTILS_H
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_UTILS_H

#include <stdint.h>
#include <string>

namespace cryptauth {

// Hashes |string| to an int64_t value. These int64_t hashes are used to fill
// the GcmDeviceInfo proto to upload to CryptAuth.
int64_t HashStringToInt64(const std::string& string);

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_ENROLLMENT_UTILS_H
