// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ipc_file_operations.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

class IpcFileOperations::IpcWriter : public FileOperations::Writer {
 public:
  IpcWriter(std::uint64_t file_id, base::WeakPtr<SharedState> shared_state);
  ~IpcWriter() override;

  // FileOperations::Writer implementation.
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  void Cancel() override;
  State state() override;

 private:
  void OnWriteChunkResult(Callback callback,
                          protocol::FileTransferResult<Monostate> result);
  void OnCloseResult(Callback callback,
                     protocol::FileTransferResult<Monostate> result);

  State state_ = kReady;
  std::uint64_t file_id_;
  base::WeakPtr<SharedState> shared_state_;

  DISALLOW_COPY_AND_ASSIGN(IpcWriter);
};

IpcFileOperations::~IpcFileOperations() = default;

void IpcFileOperations::WriteFile(const base::FilePath& filename,
                                  WriteFileCallback callback) {
  if (!shared_state_) {
    return;
  }

  std::uint64_t file_id = shared_state_->next_file_id++;
  auto writer = std::make_unique<IpcWriter>(file_id, shared_state_);
  shared_state_->result_callbacks.emplace(
      file_id, base::BindOnce(OnWriteFileResult, std::move(writer),
                              std::move(callback)));
  shared_state_->request_handler->WriteFile(file_id, filename);
}

void IpcFileOperations::ReadFile(ReadFileCallback) {
  NOTIMPLEMENTED();
}

IpcFileOperations::SharedState::SharedState(RequestHandler* request_handler)
    : request_handler(request_handler), weak_ptr_factory(this) {}

IpcFileOperations::SharedState::~SharedState() = default;

IpcFileOperations::IpcFileOperations(base::WeakPtr<SharedState> shared_state)
    : shared_state_(std::move(shared_state)) {}

void IpcFileOperations::OnWriteFileResult(
    std::unique_ptr<IpcWriter> writer,
    WriteFileCallback callback,
    protocol::FileTransferResult<Monostate> result) {
  std::move(callback).Run(
      std::move(result).Map([&](Monostate) { return std::move(writer); }));
}

IpcFileOperationsFactory::IpcFileOperationsFactory(
    IpcFileOperations::RequestHandler* request_handler)
    : shared_state_(request_handler) {}

IpcFileOperationsFactory::~IpcFileOperationsFactory() = default;

std::unique_ptr<FileOperations>
IpcFileOperationsFactory::CreateFileOperations() {
  return base::WrapUnique(
      new IpcFileOperations(shared_state_.weak_ptr_factory.GetWeakPtr()));
}

void IpcFileOperationsFactory::OnResult(
    uint64_t file_id,
    protocol::FileTransferResult<Monostate> result) {
  auto callback_iter = shared_state_.result_callbacks.find(file_id);
  if (callback_iter != shared_state_.result_callbacks.end()) {
    IpcFileOperations::ResultCallback callback =
        std::move(callback_iter->second);
    shared_state_.result_callbacks.erase(callback_iter);
    std::move(callback).Run(std::move(result));
  }
}

IpcFileOperations::IpcWriter::IpcWriter(std::uint64_t file_id,
                                        base::WeakPtr<SharedState> shared_state)
    : file_id_(file_id), shared_state_(std::move(shared_state)) {}

IpcFileOperations::IpcWriter::~IpcWriter() {
  if (!shared_state_) {
    return;
  }

  Cancel();
  // Destroy any pending callbacks.
  auto callback_iter = shared_state_->result_callbacks.find(file_id_);
  if (callback_iter != shared_state_->result_callbacks.end()) {
    shared_state_->result_callbacks.erase(callback_iter);
  }
}

void IpcFileOperations::IpcWriter::WriteChunk(std::string data,
                                              Callback callback) {
  DCHECK_EQ(kReady, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  shared_state_->request_handler->WriteChunk(file_id_, data);
  // Unretained is sound because IpcWriter will destroy any outstanding callback
  // in its destructor.
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnWriteChunkResult,
                               base::Unretained(this), std::move(callback)));
}

void IpcFileOperations::IpcWriter::Close(Callback callback) {
  DCHECK_EQ(kReady, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  shared_state_->request_handler->Close(file_id_);
  // Unretained is sound because IpcWriter will destroy any outstanding callback
  // in its destructor.
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnCloseResult,
                               base::Unretained(this), std::move(callback)));
}

void IpcFileOperations::IpcWriter::Cancel() {
  if (!shared_state_ || state_ == kClosed || state_ == kFailed) {
    return;
  }

  shared_state_->request_handler->Cancel(file_id_);
}

FileOperations::State IpcFileOperations::IpcWriter::state() {
  return state_;
}

void IpcFileOperations::IpcWriter::OnWriteChunkResult(
    Callback callback,
    protocol::FileTransferResult<Monostate> result) {
  if (result) {
    state_ = kReady;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

void IpcFileOperations::IpcWriter::OnCloseResult(
    Callback callback,
    protocol::FileTransferResult<Monostate> result) {
  if (result) {
    state_ = kClosed;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace remoting
