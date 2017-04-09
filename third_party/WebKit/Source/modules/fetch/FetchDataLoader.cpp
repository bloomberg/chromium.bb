// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "modules/fetch/BytesConsumer.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"
#include "wtf/typed_arrays/ArrayBufferBuilder.h"
#include <memory>

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

class FetchDataLoaderAsStream final : public FetchDataLoader,
                                      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsStream);

 public:
  explicit FetchDataLoaderAsStream(Stream* out_stream)
      : out_stream_(out_stream) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    bool need_to_flush = false;
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait) {
        if (need_to_flush)
          out_stream_->Flush();
        return;
      }
      if (result == BytesConsumer::Result::kOk) {
        out_stream_->AddData(buffer, available);
        need_to_flush = true;
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
          if (need_to_flush)
            out_stream_->Flush();
          out_stream_->Finalize();
          client_->DidFetchDataLoadedStream();
          return;
        case BytesConsumer::Result::kError:
          // If the stream is aborted soon after the stream is registered
          // to the StreamRegistry, ServiceWorkerURLRequestJob may not
          // notice the error and continue waiting forever.
          // TODO(yhirano): Add new message to report the error to the
          // browser process.
          out_stream_->Abort();
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  void Cancel() override { consumer_->Cancel(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(out_stream_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  Member<Stream> out_stream_;
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

FetchDataLoader* FetchDataLoader::CreateLoaderAsStream(Stream* out_stream) {
  return new FetchDataLoaderAsStream(out_stream);
}

}  // namespace blink
