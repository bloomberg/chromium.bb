// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/core/fetch/multipart_parser.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/parsed_content_disposition.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

static const int kDefaultBufferCapacity = 32768;

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

    scoped_refptr<BlobDataHandle> blob_handle =
        consumer_->DrainAsBlobDataHandle();
    if (blob_handle) {
      DCHECK_NE(UINT64_MAX, blob_handle->size());
      if (blob_handle->GetType() != mime_type_) {
        // A new Blob is created to override the Blob's type.
        auto blob_size = blob_handle->size();
        auto blob_data = std::make_unique<BlobData>();
        blob_data->SetContentType(mime_type_);
        blob_data->AppendBlob(std::move(blob_handle), 0, blob_size);
        client_->DidFetchDataLoadedBlobHandle(
            BlobDataHandle::Create(std::move(blob_data), blob_size));
      } else {
        client_->DidFetchDataLoadedBlobHandle(std::move(blob_handle));
      }
      return;
    }

    blob_data_ = std::make_unique<BlobData>();
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

  String DebugName() const override { return "FetchDataLoaderAsBlobHandle"; }

  void Trace(blink::Visitor* visitor) override {
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
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsArrayBuffer);

 public:
  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!IsValid());
    DCHECK(!consumer_);
    client_ = client;
    buffer_ = ArrayBuffer::Create(kDefaultBufferCapacity, 1);
    bytes_used_ = 0;
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
          unsigned bytes_appended =
              Append(buffer, SafeCast<wtf_size_t>(available));
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
              DOMArrayBuffer::Create(PassArrayBuffer()));
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsArrayBuffer"; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

  bool IsValid() const { return buffer_.get(); }

  // Appending empty data is not allowed.
  unsigned Append(const char* data, unsigned length) {
    DCHECK_GT(length, 0u);

    size_t current_buffer_size = buffer_->ByteLength();

    DCHECK_LE(bytes_used_, current_buffer_size);

    size_t remaining_buffer_space = current_buffer_size - bytes_used_;

    if (length > remaining_buffer_space && !ExpandCapacity(length))
      return 0;

    memcpy(static_cast<char*>(buffer_->Data()) + bytes_used_, data, length);
    bytes_used_ += length;

    return length;
  }

  // Number of bytes currently accumulated.
  unsigned ByteLength() const { return bytes_used_; }

  // Returns the accumulated data as an ArrayBuffer instance. This transfers
  // ownership of the internal buffer, making this ArrayBufferBuilder invalid
  // for future use.
  scoped_refptr<ArrayBuffer> PassArrayBuffer() {
    DCHECK_LE(bytes_used_, buffer_->ByteLength());

    if (buffer_->ByteLength() > bytes_used_)
      buffer_ = buffer_->Slice(0, bytes_used_);
    return std::move(buffer_);
  }

 private:
  // Expands the size of m_buffer to size + m_bytesUsed bytes. Returns true
  // iff successful. If reallocation is needed, copies only data in
  // [0, m_bytesUsed) range.
  bool ExpandCapacity(unsigned size_to_increase) {
    size_t current_buffer_size = buffer_->ByteLength();

    // If the size of the buffer exceeds max of unsigned, it can't be grown any
    // more.
    if (size_to_increase > std::numeric_limits<unsigned>::max() - bytes_used_)
      return false;

    unsigned new_buffer_size = bytes_used_ + size_to_increase;

    // Grow exponentially if possible.
    unsigned exponential_growth_new_buffer_size =
        std::numeric_limits<unsigned>::max();
    if (current_buffer_size <= std::numeric_limits<unsigned>::max() / 2) {
      exponential_growth_new_buffer_size =
          static_cast<unsigned>(current_buffer_size * 2);
    }
    if (exponential_growth_new_buffer_size > new_buffer_size)
      new_buffer_size = exponential_growth_new_buffer_size;

    // Copy existing data in current buffer to new buffer.
    scoped_refptr<ArrayBuffer> new_buffer =
        ArrayBuffer::Create(new_buffer_size, 1);
    if (!new_buffer)
      return false;

    memcpy(new_buffer->Data(), buffer_->Data(), bytes_used_);
    buffer_ = new_buffer;
    return true;
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  unsigned bytes_used_;
  scoped_refptr<ArrayBuffer> buffer_;
};

class FetchDataLoaderAsFailure final : public FetchDataLoader,
                                       public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsFailure);

 public:
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
    while (true) {
      const char* buffer;
      size_t available;
      auto result = consumer_->BeginRead(&buffer, &available);
      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk)
        result = consumer_->EndRead(available);
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsFailure"; }

  void Cancel() override { consumer_->Cancel(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
};

class FetchDataLoaderAsFormData final : public FetchDataLoader,
                                        public BytesConsumer::Client,
                                        public MultipartParser::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsFormData);

 public:
  explicit FetchDataLoaderAsFormData(const String& multipart_boundary)
      : multipart_boundary_(multipart_boundary) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);
    DCHECK(!form_data_);
    DCHECK(!multipart_parser_);

    StringUTF8Adaptor multipart_boundary_utf8(multipart_boundary_);
    Vector<char> multipart_boundary_vector;
    multipart_boundary_vector.Append(
        multipart_boundary_utf8.data(),
        multipart_boundary_utf8.size());

    client_ = client;
    form_data_ = MakeGarbageCollected<FormData>();
    multipart_parser_ = MakeGarbageCollected<MultipartParser>(
        std::move(multipart_boundary_vector), this);
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
        const bool buffer_appended =
            multipart_parser_->AppendData(buffer, available);
        const bool multipart_receive_failed = multipart_parser_->IsCancelled();
        result = consumer_->EndRead(available);
        if (!buffer_appended || multipart_receive_failed)
          result = BytesConsumer::Result::kError;
      }
      switch (result) {
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kDone:
          if (multipart_parser_->Finish()) {
            DCHECK(!multipart_parser_->IsCancelled());
            client_->DidFetchDataLoadedFormData(form_data_);
          } else {
            client_->DidFetchDataLoadFailed();
          }
          return;
        case BytesConsumer::Result::kError:
          client_->DidFetchDataLoadFailed();
          return;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsFormData"; }

  void Cancel() override {
    consumer_->Cancel();
    multipart_parser_->Cancel();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(form_data_);
    visitor->Trace(multipart_parser_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
    MultipartParser::Client::Trace(visitor);
  }

 private:
  void PartHeaderFieldsInMultipartReceived(
      const HTTPHeaderMap& header_fields) override {
    if (!current_entry_.Initialize(header_fields))
      multipart_parser_->Cancel();
  }

  void PartDataInMultipartReceived(const char* bytes, size_t size) override {
    if (!current_entry_.AppendBytes(bytes, size))
      multipart_parser_->Cancel();
  }

  void PartDataInMultipartFullyReceived() override {
    if (!current_entry_.Finish(form_data_))
      multipart_parser_->Cancel();
  }

  class Entry {
   public:
    bool Initialize(const HTTPHeaderMap& header_fields) {
      const ParsedContentDisposition disposition(
          header_fields.Get(http_names::kContentDisposition));
      const String disposition_type = disposition.Type();
      filename_ = disposition.Filename();
      name_ = disposition.ParameterValueForName("name");
      blob_data_.reset();
      string_builder_.reset();
      if (disposition_type != "form-data" || name_.IsNull())
        return false;
      if (!filename_.IsNull()) {
        blob_data_ = std::make_unique<BlobData>();
        const AtomicString& content_type =
            header_fields.Get(http_names::kContentType);
        blob_data_->SetContentType(content_type.IsNull() ? "text/plain"
                                                         : content_type);
      } else {
        if (!string_decoder_) {
          string_decoder_ = std::make_unique<TextResourceDecoder>(
              TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());
        }
        string_builder_.reset(new StringBuilder);
      }
      return true;
    }

    bool AppendBytes(const char* bytes, size_t size) {
      if (blob_data_)
        blob_data_->AppendBytes(bytes, size);
      if (string_builder_) {
        string_builder_->Append(string_decoder_->Decode(bytes, size));
        if (string_decoder_->SawError())
          return false;
      }
      return true;
    }

    bool Finish(FormData* form_data) {
      if (blob_data_) {
        DCHECK(!string_builder_);
        const auto size = blob_data_->length();
        File* file =
            File::Create(filename_, InvalidFileTime(),
                         BlobDataHandle::Create(std::move(blob_data_), size));
        form_data->append(name_, file, filename_);
        return true;
      }
      DCHECK(!blob_data_);
      DCHECK(string_builder_);
      string_builder_->Append(string_decoder_->Flush());
      if (string_decoder_->SawError())
        return false;
      form_data->append(name_, string_builder_->ToString());
      return true;
    }

   private:
    std::unique_ptr<BlobData> blob_data_;
    String filename_;
    String name_;
    std::unique_ptr<StringBuilder> string_builder_;
    std::unique_ptr<TextResourceDecoder> string_decoder_;
  };

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  Member<FormData> form_data_;
  Member<MultipartParser> multipart_parser_;

  Entry current_entry_;
  String multipart_boundary_;
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
    decoder_ = std::make_unique<TextResourceDecoder>(
        TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText());
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

  String DebugName() const override { return "FetchDataLoaderAsString"; }

  void Cancel() override { consumer_->Cancel(); }

  void Trace(blink::Visitor* visitor) override {
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
  USING_PRE_FINALIZER(FetchDataLoaderAsDataPipe, Dispose);

 public:
  explicit FetchDataLoaderAsDataPipe(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : data_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                           std::move(task_runner)) {}
  ~FetchDataLoaderAsDataPipe() override {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!client_);
    DCHECK(!consumer_);

    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);

    // First, try to drain the underlying mojo::DataPipe from the consumer
    // directly.  If this succeeds, all we need to do here is watch for
    // the pipe to be closed to signal completion.
    mojo::ScopedDataPipeConsumerHandle pipe_consumer =
        consumer->DrainAsDataPipe();
    if (!pipe_consumer.is_valid()) {
      // If we cannot drain the pipe from the consumer then we must copy
      // data from the consumer into a new pipe.
      MojoCreateDataPipeOptions options;
      options.struct_size = sizeof(MojoCreateDataPipeOptions);
      options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
      options.element_num_bytes = 1;
      // Use the default pipe capacity since we don't know the total
      // data size to target.
      options.capacity_num_bytes = 0;

      MojoResult rv =
          mojo::CreateDataPipe(&options, &out_data_pipe_, &pipe_consumer);
      if (rv != MOJO_RESULT_OK) {
        StopInternal();
        client_->DidFetchDataLoadFailed();
        return;
      }
      DCHECK(out_data_pipe_.is_valid());

      data_pipe_watcher_.Watch(
          out_data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
          WTF::BindRepeating(&FetchDataLoaderAsDataPipe::OnWritable,
                             WrapWeakPersistent(this)));

      data_pipe_watcher_.ArmOrNotify();
    }

    // Give the resulting pipe consumer handle to the client.
    DCHECK(pipe_consumer.is_valid());
    client_->DidFetchDataStartedDataPipe(std::move(pipe_consumer));

    // Its possible that the consumer changes state immediately after
    // calling DrainDataPipe.  In this case we call OnStateChange()
    // to process the new state.
    if (consumer->GetPublicState() !=
        BytesConsumer::PublicState::kReadableOrWaiting)
      OnStateChange();
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
        if (available == 0) {
          result = consumer_->EndRead(0);
        } else {
          uint32_t num_bytes = SafeCast<uint32_t>(available);
          MojoResult mojo_result = out_data_pipe_->WriteData(
              buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
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

  String DebugName() const override { return "FetchDataLoaderAsDataPipe"; }

  void Cancel() override { StopInternal(); }

  void Trace(blink::Visitor* visitor) override {
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

  void Dispose() { data_pipe_watcher_.Cancel(); }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;

  mojo::ScopedDataPipeProducerHandle out_data_pipe_;
  mojo::SimpleWatcher data_pipe_watcher_;
};

}  // namespace

FetchDataLoader* FetchDataLoader::CreateLoaderAsBlobHandle(
    const String& mime_type) {
  return MakeGarbageCollected<FetchDataLoaderAsBlobHandle>(mime_type);
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsArrayBuffer() {
  return MakeGarbageCollected<FetchDataLoaderAsArrayBuffer>();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsFailure() {
  return MakeGarbageCollected<FetchDataLoaderAsFailure>();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsFormData(
    const String& multipartBoundary) {
  return MakeGarbageCollected<FetchDataLoaderAsFormData>(multipartBoundary);
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsString() {
  return MakeGarbageCollected<FetchDataLoaderAsString>();
}

FetchDataLoader* FetchDataLoader::CreateLoaderAsDataPipe(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return MakeGarbageCollected<FetchDataLoaderAsDataPipe>(
      std::move(task_runner));
}

}  // namespace blink
