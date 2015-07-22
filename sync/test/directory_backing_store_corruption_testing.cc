// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/directory_backing_store_corruption_testing.h"

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"

namespace syncer {
namespace syncable {
namespace corruption_testing {

// This value needs to be large enough to force the underlying DB to be read
// from disk before writing, but small enough so that tests don't take too long
// and timeout. The value depend on the underlying DB page size as well as the
// DB's cache_size PRAGMA. If test fails, you either increase
// kNumEntriesRequiredForCorruption, or increase the size of each entry.
const int kNumEntriesRequiredForCorruption = 2000;

bool CorruptDatabase(const base::FilePath& backing_file_path) {
  // Corrupt the DB by write a bunch of zeros at the beginning.
  //
  // Because the file may already open for writing, it's important that we open
  // it in a sharing-compatible way for platforms that have the concept of
  // shared/exclusive file access (e.g. Windows).
  base::ScopedFILE db_file(base::OpenFile(backing_file_path, "wb"));
  if (!db_file.get())
    return false;
  const std::string zeros(4096, '\0');
  const int num_written = fwrite(zeros.data(), zeros.size(), 1, db_file.get());
  return num_written == 1U;
}

}  // namespace corruption_util
}  // namespace syncable
}  // namespace syncer
