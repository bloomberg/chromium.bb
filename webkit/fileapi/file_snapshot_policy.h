// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SNAPSHOT_POLICY_H_
#define WEBKIT_FILEAPI_FILE_SNAPSHOT_POLICY_H_

namespace fileapi {

// A policy flag for CreateSnapshotFile.
enum SnapshotFilePolicy {
  kSnapshotFileUnknown,

  // The implementation just uses the local file as the snapshot file.
  // The FileAPI backend does nothing on the returned file.
  kSnapshotFileLocal,

  // The implementation returns a temporary file as the snapshot file.
  // The FileAPI backend takes care of the lifetime of the returned file
  // and will delete when the last reference of the file is dropped.
  kSnapshotFileTemporary,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SNAPSHOT_POLICY_H_
