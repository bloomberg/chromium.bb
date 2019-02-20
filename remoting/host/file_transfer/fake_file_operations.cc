// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/fake_file_operations.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace remoting {

class FakeFileOperations::FakeFileWriter : public FileOperations::Writer {
 public:
  FakeFileWriter(TestIo* test_io);
  ~FakeFileWriter() override;

  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  FileOperations::State state() const override;

 private:
  void DoOpen(Callback callback);
  void DoWrite(std::string data, Callback callback);
  void DoClose(Callback callback);
  FileOperations::State state_ = FileOperations::kCreated;
  TestIo* test_io_;
  base::FilePath filename_;
  std::vector<std::string> chunks_;
  base::WeakPtrFactory<FakeFileWriter> weak_ptr_factory_;
};

std::unique_ptr<FileOperations::Reader> FakeFileOperations::CreateReader() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<FileOperations::Writer> FakeFileOperations::CreateWriter() {
  return std::make_unique<FakeFileWriter>(test_io_);
}

FakeFileOperations::FakeFileOperations(FakeFileOperations::TestIo* test_io)
    : test_io_(test_io) {}

FakeFileOperations::~FakeFileOperations() = default;

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

FakeFileOperations::FakeFileWriter::FakeFileWriter(TestIo* test_io)
    : test_io_(test_io), weak_ptr_factory_(this) {}

FakeFileOperations::FakeFileWriter::~FakeFileWriter() {
  if (state_ == FileOperations::kCreated ||
      state_ == FileOperations::kComplete ||
      state_ == FileOperations::kFailed) {
    return;
  }

  test_io_->files_written.push_back(
      OutputFile(filename_, true /* failed */, std::move(chunks_)));
}

void FakeFileOperations::FakeFileWriter::Open(const base::FilePath& filename,
                                              Callback callback) {
  CHECK_EQ(kCreated, state_) << "Open called twice";
  state_ = kBusy;
  filename_ = filename;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoOpen, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
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

FileOperations::State FakeFileOperations::FakeFileWriter::state() const {
  return state_;
}

void FakeFileOperations::FakeFileWriter::DoOpen(Callback callback) {
  if (state_ == kFailed) {
    return;
  }
  if (!test_io_->io_error) {
    state_ = kReady;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    std::move(callback).Run(*test_io_->io_error);
  }
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
    state_ = kComplete;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    test_io_->files_written.push_back(
        OutputFile(filename_, true /* failed */, std::move(chunks_)));
    std::move(callback).Run(*test_io_->io_error);
  }
}

}  // namespace remoting
