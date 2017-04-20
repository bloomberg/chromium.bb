// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include <memory>
#include "core/html/parser/TextResourceDecoder.h"
#include "modules/fetch/BytesConsumer.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBufferBuilder.h"

namespace blink {

namespace {

class FetchDataLoaderAsBlobHandle final : public FetchDataLoader,
                                          public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsBlobHandle);

 public:
  explicit FetchDataLoaderAsBlobHandle(const String& mime_type)
      : mime_type_(mime_type) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);

    client_ = client;
    consumer_ = consumer;

    RefPtr<BlobDataHandle> blob_handle = consumer_->DrainAsBlobDataHandle();
    if (blob_handle) {
      DCHECK_NE(UINT64_MAX, blob_handle->size());
      if (blob_handle->GetType() != mime_type_) {
        // A new BlobDataHandle is created to override the Blob's type.
        client_->DidFetchDataLoadedBlobHandle(BlobDataHandle::Create(
            blob_handle->Uuid(), mime_type_, blob_handle->size()));
      } else {
        client_->DidFetchDataLoadedBlobHandle(std::move(blob_handle));
      }
      return;
    }

    blob_data_ = BlobData::Create();
    blob_data_->SetContentType(mime_type_);
    consumer_->SetClient(this);
    OnStateChange();
  }

  void Cancel() override { consumer_->Cancel(); }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        blob_data_->AppendBytes(buffer, available);
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone: {
          auto size = blob_data_->length();
          client_->DidFetchDataLoadedBlobHandle(
              BlobDataHandle::Create(std::move(blob_data_), size));
          return;
        }
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  String mime_type_;
  std::unique_ptr<BlobData> blob_data_;
};

class FetchDataLoaderAsArrayBuffer final : public FetchDataLoader,
                                           public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsArrayBuffer)
 public:
  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!raw_data_);
    DCHECK(!consumer_);
    client_ = client;
    raw_data_ = WTF::MakeUnique<ArrayBufferBuilder>();
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void Cancel() override { consumer_->Cancel(); }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0) {
          unsigned bytes_appended = raw_data_->Append(buffer, available);
          if (!bytes_appended) {
            auto unused = consumer_->EndRead(0);
            ALLOW_UNUSED_LOCAL(unused);
            consumer_->Cancel();
            client_->DidFetchDataLoadFailed();
            return;
          }
          DCHECK_EQ(bytes_appended, available);
        }
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
          client_->DidFetchDataLoadedArrayBuffer(
              DOMArrayBuffer::Create(raw_data_->ToArrayBuffer()));
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  std::unique_ptr<ArrayBufferBuilder> raw_data_;
};

class FetchDataLoaderAsString final : public FetchDataLoader,
                                      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsString);

 public:
  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!decoder_);
    DCHECK(!consumer_);
    client_ = client;
    decoder_ = TextResourceDecoder::CreateAlwaysUseUTF8ForText();
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0)
          builder_.Append(decoder_->Decode(buffer, available));
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
          builder_.Append(decoder_->Flush());
          client_->DidFetchDataLoadedString(builder_.ToString());
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  void Cancel() override { consumer_->Cancel(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  std::unique_ptr<TextResourceDecoder> decoder_;
  StringBuilder builder_;
};

class FetchDataLoaderAsDataPipe final : public FetchDataLoader,
                                        public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsDataPipe);

 public:
  explicit FetchDataLoaderAsDataPipe(
      mojo::ScopedDataPipeProducerHandle out_data_pipe)
      : out_data_pipe_(std::move(out_data_pipe)),
        data_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}
  ~FetchDataLoaderAsDataPipe() override {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    data_pipe_watcher_.Watch(
        out_data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        ConvertToBaseCallback(WTF::Bind(&FetchDataLoaderAsDataPipe::OnWritable,
                                        WrapWeakPersistent(this))));
    data_pipe_watcher_.ArmOrNotify();
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
  }

  void OnWritable(MojoResult) { OnStateChange(); }

  // Implements BytesConsumer::Client.
  void OnStateChange() override {
    bool should_wait = false;
    while (!should_wait) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        DCHECK_GT(available, 0UL);
        uint32_t num_bytes = available;
        MojoResult mojo_result =
            mojo::WriteDataRaw(out_data_pipe_.get(), buffer, &num_bytes,
                               MOJO_WRITE_DATA_FLAG_NONE);
        if (mojo_result == MOJO_RESULT_OK) {
          result = consumer_->EndRead(num_bytes);
        } else if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
          result = consumer_->EndRead(0);
          should_wait = true;
          data_pipe_watcher_.ArmOrNotify();
        } else {
          result = consumer_->EndRead(0);
          StopInternal();
          client_->DidFetchDataLoadFailed();
          return;
        }
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
          StopInternal();
          client_->DidFetchDataLoadedDataPipe();
          return;
        case BytesConsumer::Result::kError:
          StopInternal();
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  void Cancel() override { StopInternal(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  void StopInternal() {
    consumer_->Cancel();
    data_pipe_watcher_.Cancel();
    out_data_pipe_.reset();
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  mojo::ScopedDataPipeProducerHandle out_data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;
};

}  // namespace

FetchDataLoader* FetchDataLoader::CreateLoaderAsBlobHandle(
    const String& mime_type) {
  return new FetchDataLoaderAsBlobHandle(mime_type);
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsArrayBuffer() {
  return new FetchDataLoaderAsArrayBuffer();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsString() {
  return new FetchDataLoaderAsString();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsDataPipe(
    mojo::ScopedDataPipeProducerHandle out_data_pipe) {
  return new FetchDataLoaderAsDataPipe(std::move(out_data_pipe));
}

}  // namespace blink
