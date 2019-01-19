// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/broadcasting_receiver.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace video_capture {

namespace {

class ConsumerAccessPermission : public mojom::ScopedAccessPermission {
 public:
  ConsumerAccessPermission(base::OnceClosure destruction_cb)
      : destruction_cb_(std::move(destruction_cb)) {}
  ~ConsumerAccessPermission() override { std::move(destruction_cb_).Run(); }

 private:
  base::OnceClosure destruction_cb_;
};

}  // anonymous namespace

BroadcastingReceiver::BufferContext::BufferContext(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle)
    : buffer_id_(buffer_id),
      buffer_handle_(std::move(buffer_handle)),
      consumer_hold_count_(0) {}

BroadcastingReceiver::BufferContext::~BufferContext() = default;

BroadcastingReceiver::BufferContext::BufferContext(
    BroadcastingReceiver::BufferContext&& other) = default;

BroadcastingReceiver::BufferContext& BroadcastingReceiver::BufferContext::
operator=(BroadcastingReceiver::BufferContext&& other) = default;

void BroadcastingReceiver::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void BroadcastingReceiver::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    access_permission_.reset();
  }
}

bool BroadcastingReceiver::BufferContext::IsStillBeingConsumed() const {
  return consumer_hold_count_ > 0;
}

media::mojom::VideoBufferHandlePtr
BroadcastingReceiver::BufferContext::CloneBufferHandle() {
  // Unable to use buffer_handle_->Clone(), because shared_buffer does not
  // support the copy constructor.
  media::mojom::VideoBufferHandlePtr result =
      media::mojom::VideoBufferHandle::New();
  if (buffer_handle_->is_shared_buffer_handle()) {
    // Special behavior here: If the handle was already read-only, the Clone()
    // call here will maintain that read-only permission. If it was read-write,
    // the cloned handle will have read-write permission.
    //
    // TODO(crbug.com/797470): We should be able to demote read-write to
    // read-only permissions when Clone()'ing handles. Currently, this causes a
    // crash.
    result->set_shared_buffer_handle(
        buffer_handle_->get_shared_buffer_handle()->Clone(
            mojo::SharedBufferHandle::AccessMode::READ_WRITE));
  } else if (buffer_handle_->is_mailbox_handles()) {
    result->set_mailbox_handles(buffer_handle_->get_mailbox_handles()->Clone());
  } else if (buffer_handle_->is_shared_memory_via_raw_file_descriptor()) {
    auto sub_struct = media::mojom::SharedMemoryViaRawFileDescriptor::New();
    sub_struct->shared_memory_size_in_bytes =
        buffer_handle_->get_shared_memory_via_raw_file_descriptor()
            ->shared_memory_size_in_bytes;
    sub_struct->file_descriptor_handle = mojo::ScopedHandle(
        buffer_handle_->get_shared_memory_via_raw_file_descriptor()
            ->file_descriptor_handle.get());
    result->set_shared_memory_via_raw_file_descriptor(std::move(sub_struct));
  } else {
    NOTREACHED() << "Unexpected video buffer handle type";
  }
  return result;
}

BroadcastingReceiver::BroadcastingReceiver()
    : status_(Status::kOnStartedHasNotYetBeenCalled),
      error_(media::VideoCaptureError::kNone),
      next_client_id_(0),
      weak_factory_(this) {}

BroadcastingReceiver::~BroadcastingReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int32_t BroadcastingReceiver::AddClient(mojom::ReceiverPtr client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto client_id = next_client_id_++;
  clients_.insert(std::make_pair(client_id, std::move(client)));
  auto& added_client = clients_[client_id];
  added_client.set_connection_error_handler(
      base::BindOnce(&BroadcastingReceiver::OnClientDisconnected,
                     weak_factory_.GetWeakPtr(), client_id));
  if (status_ == Status::kOnErrorHasBeenCalled) {
    added_client->OnError(error_);
    return client_id;
  }
  if (status_ == Status::kOnStartedHasBeenCalled)
    added_client->OnStarted();
  if (status_ == Status::kOnStartedUsingGpuDecodeHasBeenCalled)
    added_client->OnStartedUsingGpuDecode();

  for (auto& buffer_context : buffer_contexts_) {
    added_client->OnNewBuffer(buffer_context.buffer_id(),
                              buffer_context.CloneBufferHandle());
  }
  return client_id;
}

mojom::ReceiverPtr BroadcastingReceiver::RemoveClient(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto client = std::move(clients_[client_id]);
  clients_.erase(client_id);
  return client;
}

void BroadcastingReceiver::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  buffer_contexts_.emplace_back(buffer_id, std::move(buffer_handle));
  auto& buffer_context = buffer_contexts_.back();
  for (auto& client : clients_) {
    client.second->OnNewBuffer(buffer_context.buffer_id(),
                               buffer_context.CloneBufferHandle());
  }
}

void BroadcastingReceiver::OnFrameReadyInBuffer(
    int32_t buffer_id,
    int32_t frame_feedback_id,
    mojom::ScopedAccessPermissionPtr access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (clients_.empty())
    return;
  auto& buffer_context = LookupBufferContextFromBufferId(buffer_id);
  buffer_context.set_access_permission(std::move(access_permission));
  for (auto& client : clients_) {
    mojom::ScopedAccessPermissionPtr consumer_access_permission;
    mojo::MakeStrongBinding(
        std::make_unique<ConsumerAccessPermission>(base::BindOnce(
            &BroadcastingReceiver::OnClientFinishedConsumingFrame,
            weak_factory_.GetWeakPtr(), buffer_context.buffer_id())),
        mojo::MakeRequest(&consumer_access_permission));
    client.second->OnFrameReadyInBuffer(
        buffer_context.buffer_id(), frame_feedback_id,
        std::move(consumer_access_permission), frame_info.Clone());
    buffer_context.IncreaseConsumerCount();
  }
}

void BroadcastingReceiver::OnBufferRetired(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& buffer_context = LookupBufferContextFromBufferId(buffer_id);
  CHECK(!buffer_context.IsStillBeingConsumed());
  for (auto& client : clients_) {
    client.second->OnBufferRetired(buffer_id);
  }
}

void BroadcastingReceiver::OnError(media::VideoCaptureError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second->OnError(error);
  }
  status_ = Status::kOnErrorHasBeenCalled;
  error_ = error;
}

void BroadcastingReceiver::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second->OnFrameDropped(reason);
  }
}

void BroadcastingReceiver::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second->OnLog(message);
  }
}

void BroadcastingReceiver::OnStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second->OnStarted();
  }
  status_ = Status::kOnStartedHasBeenCalled;
}

void BroadcastingReceiver::OnStartedUsingGpuDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& client : clients_) {
    client.second->OnStartedUsingGpuDecode();
  }
  status_ = Status::kOnStartedUsingGpuDecodeHasBeenCalled;
}

void BroadcastingReceiver::OnClientFinishedConsumingFrame(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& buffer_context = LookupBufferContextFromBufferId(buffer_id);
  buffer_context.DecreaseConsumerCount();
}

void BroadcastingReceiver::OnClientDisconnected(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clients_.erase(client_id);
}

BroadcastingReceiver::BufferContext&
BroadcastingReceiver::LookupBufferContextFromBufferId(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto context_iter =
      std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                   [buffer_id](const BufferContext& entry) {
                     return entry.buffer_id() == buffer_id;
                   });
  return *context_iter;
}

}  // namespace video_capture
