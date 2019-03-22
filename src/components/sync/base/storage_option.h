// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_STORAGE_OPTION_H_
#define COMPONENTS_SYNC_BASE_STORAGE_OPTION_H_

namespace syncer {

// Passed as an argument when configuring sync / individual data types.
enum StorageOption { STORAGE_ON_DISK, STORAGE_IN_MEMORY };

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_STORAGE_OPTION_H_
