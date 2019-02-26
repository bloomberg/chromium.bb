// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_LOCAL_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_LOCAL_FILE_OPERATIONS_H_

#include "remoting/host/file_transfer/file_operations.h"

namespace remoting {

// Implementation of FileOperations that uses base::FileProxy to perform file
// operations on a dedicated thread.

class LocalFileOperations : public FileOperations {
 public:
  void WriteFile(const base::FilePath& filename,
                 WriteFileCallback callback) override;
  void ReadFile(ReadFileCallback) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_LOCAL_FILE_OPERATIONS_H_
