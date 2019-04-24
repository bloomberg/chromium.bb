// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_disk_info_helper.h"

namespace storage {

QuotaDiskInfoHelper::~QuotaDiskInfoHelper() = default;

int64_t QuotaDiskInfoHelper::AmountOfTotalDiskSpace(
    const base::FilePath& path) const {
  return base::SysInfo::AmountOfTotalDiskSpace(path);
}

}  // namespace storage
