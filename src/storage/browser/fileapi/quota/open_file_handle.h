// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_
#define STORAGE_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class FilePath;
}

namespace storage {

class QuotaReservation;
class OpenFileHandleContext;
class QuotaReservationBuffer;

// Represents an open file like a file descriptor.
// This should be alive while a consumer keeps a file opened and should be
// deleted when the plugin closes the file.
class STORAGE_EXPORT OpenFileHandle {
 public:
  ~OpenFileHandle();

  // Updates cached file size and consumes quota for that.
  // Both this and AddAppendModeWriteAmount should be called for each modified
  // file before calling QuotaReservation::RefreshQuota and before closing the
  // file.
  void UpdateMaxWrittenOffset(int64_t offset);

  // Notifies that |amount| of data is written to the file in append mode, and
  // consumes quota for that.
  // Both this and UpdateMaxWrittenOffset should be called for each modified
  // file before calling QuotaReservation::RefreshQuota and before closing the
  // file.
  void AddAppendModeWriteAmount(int64_t amount);

  // Returns the estimated file size for the quota consumption calculation.
  // The client must consume its reserved quota when it writes data to the file
  // beyond the estimated file size.
  // The estimated file size is greater than or equal to actual file size after
  // all clients report their file usage,  and is monotonically increasing over
  // OpenFileHandle object life cycle, so that client may cache the value.
  int64_t GetEstimatedFileSize() const;

  int64_t GetMaxWrittenOffset() const;
  const base::FilePath& platform_path() const;

 private:
  friend class QuotaReservationBuffer;

  OpenFileHandle(QuotaReservation* reservation,
                 OpenFileHandleContext* context);

  scoped_refptr<QuotaReservation> reservation_;
  scoped_refptr<OpenFileHandleContext> context_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(OpenFileHandle);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_
