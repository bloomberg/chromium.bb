// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_DIRECTORY_BACKING_STORE_CORRUPTION_TESTING_H_
#define SYNC_TEST_DIRECTORY_BACKING_STORE_CORRUPTION_TESTING_H_

#include "base/files/file_path.h"

namespace syncer {
namespace syncable {
namespace corruption_testing {

// The number of DB entries required to reliably trigger detectable corruption
// using |CorruptDatabase|.
extern const int kNumEntriesRequiredForCorruption;

// Corrupt the sync database at |backing_file_path|.
//
// Returns true if the database was successfully corrupted.
bool CorruptDatabase(const base::FilePath& backing_file_path);

}  // namespace corruption_util
}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_TEST_DIRECTORY_BACKING_STORE_CORRUPTION_TESTING_H_
