// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_types.h"

namespace dom_storage {

LocalStorageUsageInfo::LocalStorageUsageInfo()
    : data_size(0) {}
LocalStorageUsageInfo::~LocalStorageUsageInfo() {}

SessionStorageUsageInfo::SessionStorageUsageInfo() {}
SessionStorageUsageInfo::~SessionStorageUsageInfo() {}

}  // namespace dom_storage
