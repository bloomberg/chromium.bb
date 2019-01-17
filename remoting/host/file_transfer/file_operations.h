// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_

#include <cstddef>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace base {
class FilePath;
}

namespace remoting {

// Interface for reading and writing file transfers.

class FileOperations {
 public:
  enum State {
    // The file has been opened. WriteChunk(), ReadChunk(), and Close() can be
    // called.
    kReady = 0,

    // A file operation is currently being processed. WriteChunk(), ReadChunk(),
    // and Close() cannot be called until the state changes back to kReady.
    kBusy = 1,

    // Close() has been called and succeeded.
    kClosed = 2,

    // Cancel() has been called or an error occured.
    kFailed = 3,
  };

  class Writer {
   public:
    using Callback = base::OnceCallback<void(
        protocol::FileTransferResult<Monostate> result)>;

    // Destructing FileWriter before calling Close will implicitly call Cancel.
    virtual ~Writer() {}
    // Writes a chuck to the file. Chunks cannot be queued; the caller must
    // wait until callback is called before calling WriteChunk again or calling
    // Close.
    virtual void WriteChunk(std::string data, Callback callback) = 0;
    // Closes the file, flushing any data still in the OS buffer and moving the
    // the file to its final location.
    virtual void Close(Callback callback) = 0;
    // Cancels writing the file. The partially written file will be deleted. May
    // be called at any time (including when an operation is pending).
    virtual void Cancel() = 0;
    virtual State state() = 0;
  };

  class Reader {
   public:
    // On success, |result| will contain the read data.
    using Callback = base::OnceCallback<void(
        protocol::FileTransferResult<std::string> result)>;

    virtual ~Reader() {}
    // Reads a chunk of the given size from the file.
    virtual void ReadChunk(std::size_t size, Callback callback) = 0;
    virtual void Close() = 0;
    virtual State state() = 0;
  };

  // On success, |result| will contain a Writer that can be used to write data
  // to the file.
  using WriteFileCallback = base::OnceCallback<void(
      protocol::FileTransferResult<std::unique_ptr<Writer>> result)>;
  // On success, |result| will contain a Reader that can be used to read data
  // from the file.
  using ReadFileCallback = base::OnceCallback<void(
      protocol::FileTransferResult<std::unique_ptr<Reader>> result)>;

  virtual ~FileOperations() {}

  // Starts writing a new file to the default location. This will create a temp
  // file at the location, which will be renamed when writing is complete.
  virtual void WriteFile(const base::FilePath& filename,
                         WriteFileCallback callback) = 0;
  // Prompt the user to select a file and start reading it.
  virtual void ReadFile(ReadFileCallback) = 0;
};
}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_
