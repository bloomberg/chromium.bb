// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <algorithm>
#include <utility>
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

constexpr size_t ResponseBodyLoader::kMaxNumConsumedBytesInTask;

ResponseBodyLoader::ResponseBodyLoader(
    BytesConsumer& bytes_consumer,
    ResponseBodyLoaderClient& client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : bytes_consumer_(bytes_consumer),
      client_(client),
      task_runner_(std::move(task_runner)) {
  bytes_consumer_->SetClient(this);
}

mojo::ScopedDataPipeConsumerHandle ResponseBodyLoader::DrainAsDataPipe(
    ResponseBodyLoaderClient** client) {
  DCHECK(!started_);

  *client = nullptr;
  if (drained_ || aborted_) {
    return {};
  }

  DCHECK(bytes_consumer_);
  auto data_pipe = bytes_consumer_->DrainAsDataPipe();

  if (data_pipe) {
    drained_ = true;
    bytes_consumer_ = nullptr;
    *client = this;
  }

  return data_pipe;
}

void ResponseBodyLoader::DidReceiveData(base::span<const char> data) {
  if (aborted_)
    return;

  client_->DidReceiveData(data);
}

void ResponseBodyLoader::DidFinishLoadingBody() {
  if (aborted_)
    return;

  if (suspended_) {
    finish_signal_is_pending_ = true;
    return;
  }

  finish_signal_is_pending_ = false;
  client_->DidFinishLoadingBody();
}

void ResponseBodyLoader::DidFailLoadingBody() {
  if (aborted_)
    return;

  if (suspended_) {
    fail_signal_is_pending_ = true;
    return;
  }

  fail_signal_is_pending_ = false;
  client_->DidFailLoadingBody();
}

void ResponseBodyLoader::Start() {
  DCHECK(!started_);
  DCHECK(!drained_);

  started_ = true;
  OnStateChange();
}

void ResponseBodyLoader::Abort() {
  if (aborted_)
    return;

  aborted_ = true;

  if (bytes_consumer_ && !in_two_phase_read_)
    bytes_consumer_->Cancel();
}

void ResponseBodyLoader::Suspend() {
  if (aborted_)
    return;

  DCHECK(!suspended_);
  suspended_ = true;
}

void ResponseBodyLoader::Resume() {
  if (aborted_)
    return;

  DCHECK(suspended_);
  suspended_ = false;

  if (finish_signal_is_pending_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFinishLoadingBody,
                                  WrapPersistent(this)));
  } else if (fail_signal_is_pending_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ResponseBodyLoader::DidFailLoadingBody,
                                  WrapPersistent(this)));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                          WrapPersistent(this)));
  }
}

void ResponseBodyLoader::OnStateChange() {
  if (!started_)
    return;

  size_t num_bytes_consumed = 0;

  while (!aborted_ && !suspended_) {
    if (kMaxNumConsumedBytesInTask == num_bytes_consumed) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&ResponseBodyLoader::OnStateChange,
                                            WrapPersistent(this)));
      return;
    }

    const char* buffer = nullptr;
    size_t available = 0;
    auto result = bytes_consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    if (result == BytesConsumer::Result::kOk) {
      in_two_phase_read_ = true;

      available =
          std::min(available, kMaxNumConsumedBytesInTask - num_bytes_consumed);
      DidReceiveData(base::make_span(buffer, available));
      result = bytes_consumer_->EndRead(available);
      in_two_phase_read_ = false;
      num_bytes_consumed += available;

      if (aborted_) {
        // As we cannot call Cancel in two-phase read, we need to call it here.
        bytes_consumer_->Cancel();
      }
    }
    DCHECK_NE(result, BytesConsumer::Result::kShouldWait);
    if (result == BytesConsumer::Result::kDone) {
      DidFinishLoadingBody();
      return;
    }
    if (result != BytesConsumer::Result::kOk) {
      DidFailLoadingBody();
      Abort();
      return;
    }
  }
}

void ResponseBodyLoader::Trace(Visitor* visitor) {
  visitor->Trace(bytes_consumer_);
  visitor->Trace(client_);
  ResponseBodyLoaderDrainableInterface::Trace(visitor);
  ResponseBodyLoaderClient::Trace(visitor);
  BytesConsumer::Client::Trace(visitor);
}

}  // namespace blink
