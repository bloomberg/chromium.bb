// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_
#define WEBKIT_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace fileapi {

class QuotaReservation;
class OpenFileHandleContext;
class QuotaReservationBuffer;

// Represents an open file like a file descriptor.
// This should be alive while a consumer keeps a file opened and should be
// deleted when the plugin closes the file.
class WEBKIT_STORAGE_BROWSER_EXPORT OpenFileHandle {
 public:
  ~OpenFileHandle();

  // Updates cached file size and consumes quota for that.
  // This should be called for each modified file before calling RefreshQuota
  // and file close.
  // Returns updated base file size that should be used to measure quota
  // consumption by difference to this.
  int64 UpdateMaxWrittenOffset(int64 offset);

  int64 base_file_size() const;

 private:
  friend class QuotaReservationBuffer;

  OpenFileHandle(QuotaReservation* reservation,
                 OpenFileHandleContext* context);

  scoped_refptr<QuotaReservation> reservation_;
  scoped_refptr<OpenFileHandleContext> context_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(OpenFileHandle);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_QUOTA_OPEN_FILE_HANDLE_H_
