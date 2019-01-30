// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/replaying_web_data_consumer_handle.h"

#include <utility>
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

class ReplayingWebDataConsumerHandle::ReaderImpl final : public Reader {
 public:
  ReaderImpl(scoped_refptr<Context> context, Client* client)
      : context_(std::move(context)) {
    context_->AttachReader(client);
  }
  ~ReaderImpl() override { context_->DetachReader(); }

  WebDataConsumerHandle::Result BeginRead(const void** buffer,
                                          Flags flags,
                                          size_t* available) override {
    return context_->BeginRead(buffer, flags, available);
  }
  WebDataConsumerHandle::Result EndRead(size_t read_size) override {
    return context_->EndRead(read_size);
  }

 private:
  scoped_refptr<Context> context_;
};

void ReplayingWebDataConsumerHandle::Context::Add(const Command& command) {
  MutexLocker locker(mutex_);
  commands_.push_back(command);
}

void ReplayingWebDataConsumerHandle::Context::AttachReader(
    WebDataConsumerHandle::Client* client) {
  MutexLocker locker(mutex_);
  DCHECK(!reader_thread_);
  DCHECK(!client_);
  reader_thread_ = blink::Thread::Current();
  client_ = client;

  if (client_ && !(IsEmpty() && result_ == kShouldWait))
    Notify();
}

void ReplayingWebDataConsumerHandle::Context::DetachReader() {
  MutexLocker locker(mutex_);
  DCHECK(reader_thread_ && reader_thread_->IsCurrentThread());
  reader_thread_ = nullptr;
  client_ = nullptr;
  if (!is_handle_attached_)
    detached_->Signal();
}

void ReplayingWebDataConsumerHandle::Context::DetachHandle() {
  MutexLocker locker(mutex_);
  is_handle_attached_ = false;
  if (!reader_thread_)
    detached_->Signal();
}

WebDataConsumerHandle::Result
ReplayingWebDataConsumerHandle::Context::BeginRead(const void** buffer,
                                                   Flags,
                                                   size_t* available) {
  MutexLocker locker(mutex_);
  *buffer = nullptr;
  *available = 0;
  if (IsEmpty())
    return result_;

  const Command& command = Top();
  WebDataConsumerHandle::Result result = kOk;
  switch (command.GetName()) {
    case Command::kData: {
      auto& body = command.Body();
      *available = body.size() - Offset();
      *buffer = body.data() + Offset();
      result = kOk;
      break;
    }
    case Command::kDone:
      result_ = result = kDone;
      Consume(0);
      break;
    case Command::kWait:
      Consume(0);
      result = kShouldWait;
      Notify();
      break;
    case Command::kError:
      result_ = result = kUnexpectedError;
      Consume(0);
      break;
  }
  return result;
}

WebDataConsumerHandle::Result ReplayingWebDataConsumerHandle::Context::EndRead(
    size_t read_size) {
  MutexLocker locker(mutex_);
  Consume(read_size);
  return kOk;
}

ReplayingWebDataConsumerHandle::Context::Context()
    : offset_(0),
      reader_thread_(nullptr),
      client_(nullptr),
      result_(kShouldWait),
      is_handle_attached_(true),
      detached_(std::make_unique<WaitableEvent>()) {}

const ReplayingWebDataConsumerHandle::Command&
ReplayingWebDataConsumerHandle::Context::Top() {
  DCHECK(!IsEmpty());
  return commands_.front();
}

void ReplayingWebDataConsumerHandle::Context::Consume(size_t size) {
  DCHECK(!IsEmpty());
  DCHECK(size + offset_ <= Top().Body().size());
  bool fully_consumed = (size + offset_ >= Top().Body().size());
  if (fully_consumed) {
    offset_ = 0;
    commands_.pop_front();
  } else {
    offset_ += size;
  }
}

void ReplayingWebDataConsumerHandle::Context::Notify() {
  if (!client_)
    return;
  DCHECK(reader_thread_);
  PostCrossThreadTask(
      *reader_thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBind(&Context::NotifyInternal, WrapRefCounted(this)));
}

void ReplayingWebDataConsumerHandle::Context::NotifyInternal() {
  {
    MutexLocker locker(mutex_);
    if (!client_ || !reader_thread_->IsCurrentThread()) {
      // There is no client, or a new reader is attached.
      return;
    }
  }
  // The reading thread is the current thread.
  client_->DidGetReadable();
}

ReplayingWebDataConsumerHandle::ReplayingWebDataConsumerHandle()
    : context_(Context::Create()) {}

ReplayingWebDataConsumerHandle::~ReplayingWebDataConsumerHandle() {
  context_->DetachHandle();
}

std::unique_ptr<WebDataConsumerHandle::Reader>
ReplayingWebDataConsumerHandle::ObtainReader(
    Client* client,
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return std::make_unique<ReaderImpl>(context_, client);
}

void ReplayingWebDataConsumerHandle::Add(const Command& command) {
  context_->Add(command);
}

}  // namespace blink
