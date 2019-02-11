// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/fake_file_operations.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace remoting {

class FakeFileOperations::FakeFileWriter : public FileOperations::Writer {
 public:
  FakeFileWriter(TestIo* test_io, const base::FilePath& filename);
  ~FakeFileWriter() override;

  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  void Cancel() override;
  FileOperations::State state() override;

 private:
  void DoWrite(std::string data, Callback callback);
  void DoClose(Callback callback);
  FileOperations::State state_ = FileOperations::kReady;
  TestIo* test_io_;
  base::FilePath filename_;
  std::vector<std::string> chunks_;
  base::WeakPtrFactory<FakeFileWriter> weak_ptr_factory_;
};

void FakeFileOperations::WriteFile(const base::FilePath& filename,
                                   WriteFileCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileOperations::DoWriteFile, base::Unretained(this),
                     filename, std::move(callback)));
}

void FakeFileOperations::ReadFile(ReadFileCallback) {
  NOTIMPLEMENTED();
}

FakeFileOperations::FakeFileOperations(FakeFileOperations::TestIo* test_io)
    : test_io_(test_io) {}

FakeFileOperations::~FakeFileOperations() = default;

void FakeFileOperations::DoWriteFile(const base::FilePath& filename,
                                     WriteFileCallback callback) {
  if (!test_io_->io_error) {
    std::move(callback).Run(
        std::make_unique<FakeFileWriter>(test_io_, filename));
  } else {
    std::move(callback).Run(*test_io_->io_error);
  }
}

FakeFileOperations::OutputFile::OutputFile(base::FilePath filename,
                                           bool failed,
                                           std::vector<std::string> chunks)
    : filename(std::move(filename)),
      failed(failed),
      chunks(std::move(chunks)) {}

FakeFileOperations::OutputFile::OutputFile(const OutputFile& other) = default;
FakeFileOperations::OutputFile::~OutputFile() = default;
FakeFileOperations::TestIo::TestIo() = default;
FakeFileOperations::TestIo::TestIo(const TestIo& other) = default;
FakeFileOperations::TestIo::~TestIo() = default;

FakeFileOperations::FakeFileWriter::FakeFileWriter(
    TestIo* test_io,
    const base::FilePath& filename)
    : test_io_(test_io), filename_(filename), weak_ptr_factory_(this) {}

FakeFileOperations::FakeFileWriter::~FakeFileWriter() {
  Cancel();
}

void FakeFileOperations::FakeFileWriter::WriteChunk(std::string data,
                                                    Callback callback) {
  CHECK_EQ(kReady, state_) << "WriteChunk called when writer not ready";
  state_ = kBusy;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoWrite, weak_ptr_factory_.GetWeakPtr(),
                     std::move(data), std::move(callback)));
}

void FakeFileOperations::FakeFileWriter::Close(Callback callback) {
  CHECK_EQ(kReady, state_) << "Close called when writer not ready";
  state_ = kBusy;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoClose, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void FakeFileOperations::FakeFileWriter::Cancel() {
  if (state_ == FileOperations::kClosed || state_ == FileOperations::kFailed) {
    return;
  }

  state_ = kFailed;
  test_io_->files_written.push_back(
      OutputFile(filename_, true /* failed */, std::move(chunks_)));
}

FileOperations::State FakeFileOperations::FakeFileWriter::state() {
  return state_;
}

void FakeFileOperations::FakeFileWriter::DoWrite(std::string data,
                                                 Callback callback) {
  if (state_ == kFailed) {
    return;
  }
  if (!test_io_->io_error) {
    chunks_.push_back(std::move(data));
    state_ = kReady;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    test_io_->files_written.push_back(
        OutputFile(filename_, true /* failed */, std::move(chunks_)));
    std::move(callback).Run(*test_io_->io_error);
  }
}

void FakeFileOperations::FakeFileWriter::DoClose(Callback callback) {
  if (state_ == kFailed) {
    return;
  }
  if (!test_io_->io_error) {
    test_io_->files_written.push_back(
        OutputFile(filename_, false /* failed */, std::move(chunks_)));
    state_ = kClosed;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    test_io_->files_written.push_back(
        OutputFile(filename_, true /* failed */, std::move(chunks_)));
    std::move(callback).Run(*test_io_->io_error);
  }
}

}  // namespace remoting
