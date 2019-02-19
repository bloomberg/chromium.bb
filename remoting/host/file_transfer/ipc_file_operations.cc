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
  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  State state() const override;

 private:
  void OnOperationResult(Callback callback, ResultHandler::Result result);
  void OnCloseResult(Callback callback, ResultHandler::Result result);

  State state_ = kCreated;
  std::uint64_t file_id_;
  base::WeakPtr<SharedState> shared_state_;

  DISALLOW_COPY_AND_ASSIGN(IpcWriter);
};

IpcFileOperations::~IpcFileOperations() = default;

std::unique_ptr<FileOperations::Reader> IpcFileOperations::CreateReader() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<FileOperations::Writer> IpcFileOperations::CreateWriter() {
  // If shared_state_ is invalid, it means the connection is being torn down.
  // We can still return an IpcWriter, it just won't do anything due to its own
  // checks of shared_state_. That should be okay, because our caller should be
  // torn down soon, too.
  std::uint64_t file_id = shared_state_ ? shared_state_->next_file_id++ : 0;
  return std::make_unique<IpcWriter>(file_id, shared_state_);
}

IpcFileOperations::SharedState::SharedState(RequestHandler* request_handler)
    : request_handler(request_handler), weak_ptr_factory(this) {}

IpcFileOperations::SharedState::~SharedState() = default;

IpcFileOperations::IpcFileOperations(base::WeakPtr<SharedState> shared_state)
    : shared_state_(std::move(shared_state)) {}

IpcFileOperationsFactory::IpcFileOperationsFactory(
    IpcFileOperations::RequestHandler* request_handler)
    : shared_state_(request_handler) {}

IpcFileOperationsFactory::~IpcFileOperationsFactory() = default;

std::unique_ptr<FileOperations>
IpcFileOperationsFactory::CreateFileOperations() {
  return base::WrapUnique(
      new IpcFileOperations(shared_state_.weak_ptr_factory.GetWeakPtr()));
}

void IpcFileOperationsFactory::OnResult(uint64_t file_id, Result result) {
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
  if (!shared_state_ || state_ == kCreated || state_ == kComplete ||
      state_ == kFailed) {
    return;
  }

  shared_state_->request_handler->Cancel(file_id_);

  // Destroy any pending callbacks.
  auto callback_iter = shared_state_->result_callbacks.find(file_id_);
  if (callback_iter != shared_state_->result_callbacks.end()) {
    shared_state_->result_callbacks.erase(callback_iter);
  }
}

void IpcFileOperations::IpcWriter::Open(const base::FilePath& filename,
                                        Callback callback) {
  DCHECK_EQ(kCreated, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnOperationResult,
                               base::Unretained(this), std::move(callback)));
  shared_state_->request_handler->WriteFile(file_id_, filename);
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
      file_id_, base::BindOnce(&IpcWriter::OnOperationResult,
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

FileOperations::State IpcFileOperations::IpcWriter::state() const {
  return state_;
}

void IpcFileOperations::IpcWriter::OnOperationResult(
    Callback callback,
    ResultHandler::Result result) {
  if (result) {
    state_ = kReady;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

void IpcFileOperations::IpcWriter::OnCloseResult(Callback callback,
                                                 ResultHandler::Result result) {
  if (result) {
    state_ = kComplete;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace remoting
