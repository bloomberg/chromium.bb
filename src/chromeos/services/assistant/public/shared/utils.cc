// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/shared/utils.h"

namespace chromeos {
namespace assistant {

AndroidAppInfo::AndroidAppInfo() = default;
AndroidAppInfo::AndroidAppInfo(const AndroidAppInfo& suggestion) = default;
AndroidAppInfo& AndroidAppInfo::operator=(const AndroidAppInfo&) = default;
AndroidAppInfo::AndroidAppInfo(AndroidAppInfo&& suggestion) = default;
AndroidAppInfo& AndroidAppInfo::operator=(AndroidAppInfo&&) = default;
AndroidAppInfo::~AndroidAppInfo() = default;

}  // namespace assistant
}  // namespace chromeos
