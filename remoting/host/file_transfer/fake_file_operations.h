// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/proto/file_transfer.pb.h"

namespace remoting {

// Fake FileOperations implementation for testing. Outputs written files to a
// vector.
class FakeFileOperations : public FileOperations {
 public:
  struct OutputFile {
    OutputFile(base::FilePath filename,
               bool failed,
               std::vector<std::string> chunks);
    OutputFile(const OutputFile& other);
    ~OutputFile();

    // The filename provided to WriteFile.
    base::FilePath filename;

    // True if the file was canceled or returned an error due to io_error being
    // set. False if the file was written and closed successfully.
    bool failed;

    // All of the chunks successfully written before close/cancel/error.
    std::vector<std::string> chunks;
  };

  // Used to interact with FakeFileOperations after ownership is passed
  // elsewhere.
  struct TestIo {
    TestIo();
    TestIo(const TestIo& other);
    ~TestIo();

    // An element will be added for each file written in full or in part.
    std::vector<OutputFile> files_written;

    // If set, file operations will return this error.
    base::Optional<protocol::FileTransfer_Error> io_error = base::nullopt;
  };

  explicit FakeFileOperations(TestIo* test_io);
  ~FakeFileOperations() override;

  // FileOperations implementation.
  void WriteFile(const base::FilePath& filename,
                 WriteFileCallback callback) override;
  void ReadFile(ReadFileCallback) override;

 private:
  class FakeFileWriter;

  void DoWriteFile(const base::FilePath& filename, WriteFileCallback callback);

  TestIo* test_io_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_
