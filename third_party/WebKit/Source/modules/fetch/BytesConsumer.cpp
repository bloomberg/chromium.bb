// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumer.h"

#include <string.h>
#include <algorithm>
#include "base/memory/scoped_refptr.h"
#include "core/dom/ExecutionContext.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "platform/WebTaskRunner.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Functional.h"
#include "public/platform/TaskType.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NoopClient final : public GarbageCollectedFinalized<NoopClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(NoopClient);

 public:
  void OnStateChange() override {}
  String DebugName() const override { return "NoopClient"; }
};

class TeeHelper final : public GarbageCollectedFinalized<TeeHelper>,
                        public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(TeeHelper);

 public:
  TeeHelper(ExecutionContext* execution_context, BytesConsumer* consumer)
      : src_(consumer),
        destination1_(new Destination(execution_context, this)),
        destination2_(new Destination(execution_context, this)) {
    consumer->SetClient(this);
    // As no client is set to either destinations, Destination::notify() is
    // no-op in this function.
    OnStateChange();
  }

  void OnStateChange() override {
    bool destination1_was_empty = destination1_->IsEmpty();
    bool destination2_was_empty = destination2_->IsEmpty();
    bool has_enqueued = false;

    while (true) {
      const char* buffer = nullptr;
      size_t available = 0;
      auto result = src_->BeginRead(&buffer, &available);
      if (result == Result::kShouldWait) {
        if (has_enqueued && destination1_was_empty)
          destination1_->Notify();
        if (has_enqueued && destination2_was_empty)
          destination2_->Notify();
        return;
      }
      Chunk* chunk = nullptr;
      if (result == Result::kOk) {
        chunk = new Chunk(buffer, available);
        result = src_->EndRead(available);
      }
      switch (result) {
        case Result::kOk:
          DCHECK(chunk);
          destination1_->Enqueue(chunk);
          destination2_->Enqueue(chunk);
          has_enqueued = true;
          break;
        case Result::kShouldWait:
          NOTREACHED();
          return;
        case Result::kDone:
          if (destination1_was_empty)
            destination1_->Notify();
          if (destination2_was_empty)
            destination2_->Notify();
          return;
        case Result::kError:
          ClearAndNotify();
          return;
      }
    }
  }
  String DebugName() const override { return "TeeHelper"; }

  BytesConsumer::PublicState GetPublicState() const {
    return src_->GetPublicState();
  }

  BytesConsumer::Error GetError() const { return src_->GetError(); }

  void Cancel() {
    if (!destination1_->IsCancelled() || !destination2_->IsCancelled())
      return;
    src_->Cancel();
  }

  BytesConsumer* Destination1() const { return destination1_; }
  BytesConsumer* Destination2() const { return destination2_; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(src_);
    visitor->Trace(destination1_);
    visitor->Trace(destination2_);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  using Result = BytesConsumer::Result;
  class Chunk final : public GarbageCollectedFinalized<Chunk> {
   public:
    Chunk(const char* data, size_t size) {
      buffer_.ReserveInitialCapacity(size);
      buffer_.Append(data, size);
      // Report buffer size to V8 so GC can be triggered appropriately.
      v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
          static_cast<int64_t>(buffer_.size()));
    }
    ~Chunk() {
      v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
          -static_cast<int64_t>(buffer_.size()));
    }
    const char* data() const { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }

    void Trace(blink::Visitor* visitor) {}

   private:
    Vector<char> buffer_;
  };

  class Destination final : public BytesConsumer {
   public:
    Destination(ExecutionContext* execution_context, TeeHelper* tee)
        : execution_context_(execution_context), tee_(tee) {}

    Result BeginRead(const char** buffer, size_t* available) override {
      DCHECK(!chunk_in_use_);
      *buffer = nullptr;
      *available = 0;
      if (is_cancelled_ || is_closed_)
        return Result::kDone;
      if (!chunks_.IsEmpty()) {
        Chunk* chunk = chunks_[0];
        DCHECK_LE(offset_, chunk->size());
        *buffer = chunk->data() + offset_;
        *available = chunk->size() - offset_;
        chunk_in_use_ = chunk;
        return Result::kOk;
      }
      switch (tee_->GetPublicState()) {
        case PublicState::kReadableOrWaiting:
          return Result::kShouldWait;
        case PublicState::kClosed:
          is_closed_ = true;
          ClearClient();
          return Result::kDone;
        case PublicState::kErrored:
          ClearClient();
          return Result::kError;
      }
      NOTREACHED();
      return Result::kError;
    }

    Result EndRead(size_t read) override {
      DCHECK(chunk_in_use_);
      DCHECK(chunks_.IsEmpty() || chunk_in_use_ == chunks_[0]);
      chunk_in_use_ = nullptr;
      if (chunks_.IsEmpty()) {
        // This object becomes errored during the two-phase read.
        DCHECK_EQ(PublicState::kErrored, GetPublicState());
        return Result::kOk;
      }
      Chunk* chunk = chunks_[0];
      DCHECK_LE(offset_ + read, chunk->size());
      offset_ += read;
      if (chunk->size() == offset_) {
        offset_ = 0;
        chunks_.pop_front();
      }
      if (chunks_.IsEmpty() && tee_->GetPublicState() == PublicState::kClosed) {
        // All data has been consumed.
        execution_context_->GetTaskRunner(TaskType::kNetworking)
            ->PostTask(BLINK_FROM_HERE,
                       WTF::Bind(&Destination::Close, WrapPersistent(this)));
      }
      return Result::kOk;
    }

    void SetClient(BytesConsumer::Client* client) override {
      DCHECK(!client_);
      DCHECK(client);
      auto state = GetPublicState();
      if (state == PublicState::kClosed || state == PublicState::kErrored)
        return;
      client_ = client;
    }

    void ClearClient() override {
      client_ = nullptr;
    }

    void Cancel() override {
      DCHECK(!chunk_in_use_);
      auto state = GetPublicState();
      if (state == PublicState::kClosed || state == PublicState::kErrored)
        return;
      is_cancelled_ = true;
      ClearChunks();
      ClearClient();
      tee_->Cancel();
    }

    PublicState GetPublicState() const override {
      if (is_cancelled_ || is_closed_)
        return PublicState::kClosed;
      auto state = tee_->GetPublicState();
      // We don't say this object is closed unless m_isCancelled or
      // m_isClosed is set.
      return state == PublicState::kClosed ? PublicState::kReadableOrWaiting
                                           : state;
    }

    Error GetError() const override { return tee_->GetError(); }

    String DebugName() const override { return "TeeHelper::Destination"; }

    void Enqueue(Chunk* chunk) {
      if (is_cancelled_)
        return;
      chunks_.push_back(chunk);
    }

    bool IsEmpty() const { return chunks_.IsEmpty(); }

    void ClearChunks() {
      chunks_.clear();
      offset_ = 0;
    }

    void Notify() {
      if (is_cancelled_ || is_closed_)
        return;
      if (chunks_.IsEmpty() && tee_->GetPublicState() == PublicState::kClosed) {
        Close();
        return;
      }
      if (client_) {
        client_->OnStateChange();
        if (GetPublicState() == PublicState::kErrored)
          ClearClient();
      }
    }

    bool IsCancelled() const { return is_cancelled_; }

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(execution_context_);
      visitor->Trace(tee_);
      visitor->Trace(client_);
      visitor->Trace(chunks_);
      visitor->Trace(chunk_in_use_);
      BytesConsumer::Trace(visitor);
    }

   private:
    void Close() {
      DCHECK_EQ(PublicState::kClosed, tee_->GetPublicState());
      DCHECK(chunks_.IsEmpty());
      if (is_closed_ || is_cancelled_) {
        // It's possible to reach here because this function can be
        // called asynchronously.
        return;
      }
      DCHECK_EQ(PublicState::kReadableOrWaiting, GetPublicState());
      is_closed_ = true;
      if (client_) {
        client_->OnStateChange();
        ClearClient();
      }
    }

    Member<ExecutionContext> execution_context_;
    Member<TeeHelper> tee_;
    Member<BytesConsumer::Client> client_;
    HeapDeque<Member<Chunk>> chunks_;
    Member<Chunk> chunk_in_use_;
    size_t offset_ = 0;
    bool is_cancelled_ = false;
    bool is_closed_ = false;
  };

  void ClearAndNotify() {
    destination1_->ClearChunks();
    destination2_->ClearChunks();
    destination1_->Notify();
    destination2_->Notify();
  }

  Member<BytesConsumer> src_;
  Member<Destination> destination1_;
  Member<Destination> destination2_;
};

class ErroredBytesConsumer final : public BytesConsumer {
 public:
  explicit ErroredBytesConsumer(const Error& error) : error_(error) {}

  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::kError;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kErrored; }
  Error GetError() const override { return error_; }
  String DebugName() const override { return "ErroredBytesConsumer"; }

 private:
  const Error error_;
};

class ClosedBytesConsumer final : public BytesConsumer {
 public:
  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::kDone;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kClosed; }
  Error GetError() const override {
    NOTREACHED();
    return Error();
  }
  String DebugName() const override { return "ClosedBytesConsumer"; }
};

}  // namespace

void BytesConsumer::Tee(ExecutionContext* execution_context,
                        BytesConsumer* src,
                        BytesConsumer** dest1,
                        BytesConsumer** dest2) {
  scoped_refptr<BlobDataHandle> blob_data_handle =
      src->DrainAsBlobDataHandle(BlobSizePolicy::kAllowBlobWithInvalidSize);
  if (blob_data_handle) {
    // Register a client in order to be consistent.
    src->SetClient(new NoopClient);
    *dest1 = new BlobBytesConsumer(execution_context, blob_data_handle);
    *dest2 = new BlobBytesConsumer(execution_context, blob_data_handle);
    return;
  }

  TeeHelper* tee = new TeeHelper(execution_context, src);
  *dest1 = tee->Destination1();
  *dest2 = tee->Destination2();
}

BytesConsumer* BytesConsumer::CreateErrored(const BytesConsumer::Error& error) {
  return new ErroredBytesConsumer(error);
}

BytesConsumer* BytesConsumer::CreateClosed() {
  return new ClosedBytesConsumer();
}

}  // namespace blink
