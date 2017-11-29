/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fileapi/FileReaderLoader.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebURLRequest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

FileError::ErrorCode HttpStatusCodeToErrorCode(int http_status_code) {
  switch (http_status_code) {
    case 403:
      return FileError::kSecurityErr;
    case 404:
      return FileError::kNotFoundErr;
    default:
      return FileError::kNotReadableErr;
  }
}

class FileReaderLoaderIPC final : public FileReaderLoader,
                                  public ThreadableLoaderClient {
 public:
  FileReaderLoaderIPC(ReadType read_type, FileReaderLoaderClient* client)
      : FileReaderLoader(read_type, client) {}
  ~FileReaderLoaderIPC() override {
    if (!url_for_reading_.IsEmpty())
      BlobRegistry::RevokePublicBlobURL(url_for_reading_);
  }

  void Start(ExecutionContext*, scoped_refptr<BlobDataHandle>) override;

  // ThreadableLoaderClient
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveData(const char*, unsigned) override;
  void DidFinishLoading(unsigned long, double) override;
  void DidFail(const ResourceError&) override;

 private:
  void Cleanup() override {
    if (loader_) {
      loader_->Cancel();
      loader_ = nullptr;
    }

    FileReaderLoader::Cleanup();
  }

  KURL url_for_reading_;
  Persistent<ThreadableLoader> loader_;
};

void FileReaderLoaderIPC::Start(ExecutionContext* execution_context,
                                scoped_refptr<BlobDataHandle> blob_data) {
  DCHECK(execution_context);
#if DCHECK_IS_ON()
  DCHECK(!started_loading_) << "FileReaderLoader can only be used once";
  started_loading_ = true;
#endif  // DCHECK_IS_ON()

  // The blob is read by routing through the request handling layer given a
  // temporary public url.
  url_for_reading_ =
      BlobURL::CreatePublicURL(execution_context->GetSecurityOrigin());
  if (url_for_reading_.IsEmpty()) {
    Failed(FileError::kSecurityErr);
    return;
  }

  BlobRegistry::RegisterPublicBlobURL(
      execution_context->GetMutableSecurityOrigin(), url_for_reading_,
      std::move(blob_data));
  // Construct and load the request.
  ResourceRequest request(url_for_reading_);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context->GetSecurityContext().AddressSpace());

  // FIXME: Should this really be 'internal'? Do we know anything about the
  // actual request that generated this fetch?
  request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  request.SetFetchRequestMode(network::mojom::FetchRequestMode::kSameOrigin);

  request.SetHTTPMethod(HTTPNames::GET);

  ThreadableLoaderOptions options;

  ResourceLoaderOptions resource_loader_options;
  // Use special initiator to hide the request from the inspector.
  resource_loader_options.initiator_info.name =
      FetchInitiatorTypeNames::internal;

  if (!IsSyncLoad()) {
    DCHECK(!loader_);
    loader_ = ThreadableLoader::Create(*execution_context, this, options,
                                       resource_loader_options);
    loader_->Start(request);
  } else {
    ThreadableLoader::LoadResourceSynchronously(
        *execution_context, request, *this, options, resource_loader_options);
  }
}

void FileReaderLoaderIPC::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  if (response.HttpStatusCode() != 200) {
    Failed(HttpStatusCodeToErrorCode(response.HttpStatusCode()));
    return;
  }

  OnStartLoading(response.ExpectedContentLength());
}

void FileReaderLoaderIPC::DidReceiveData(const char* data,
                                         unsigned data_length) {
  OnReceivedData(data, data_length);
}

void FileReaderLoaderIPC::DidFinishLoading(unsigned long, double) {
  OnFinishLoading();
}

void FileReaderLoaderIPC::DidFail(const ResourceError& error) {
  if (error.IsCancellation())
    return;

  Failed(FileError::kNotReadableErr);
}

class FileReaderLoaderMojo : public FileReaderLoader,
                             public mojom::blink::BlobReaderClient {
 public:
  FileReaderLoaderMojo(ReadType read_type, FileReaderLoaderClient* client)
      : FileReaderLoader(read_type, client),
        handle_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
        binding_(this) {}
  ~FileReaderLoaderMojo() override {}

  void Start(ExecutionContext*, scoped_refptr<BlobDataHandle>) override;

  // BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override;

 private:
  void Cleanup() override {
    handle_watcher_.Cancel();
    consumer_handle_.reset();

    FileReaderLoader::Cleanup();
  }

  void OnDataPipeReadable(MojoResult);

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::SimpleWatcher handle_watcher_;
  mojo::Binding<mojom::blink::BlobReaderClient> binding_;
  int64_t expected_content_size_ = -1;
  bool received_all_data_ = false;
  bool received_on_complete_ = false;
};

void FileReaderLoaderMojo::Start(ExecutionContext*,
                                 scoped_refptr<BlobDataHandle> blob_data) {
#if DCHECK_IS_ON()
  DCHECK(!started_loading_) << "FileReaderLoader can only be used once";
  started_loading_ = true;
#endif  // DCHECK_IS_ON()

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult result =
      CreateDataPipe(nullptr, &producer_handle, &consumer_handle_);
  if (result != MOJO_RESULT_OK) {
    Failed(FileError::kNotReadableErr);
    return;
  }

  mojom::blink::BlobReaderClientPtr client_ptr;
  binding_.Bind(MakeRequest(&client_ptr));
  blob_data->ReadAll(std::move(producer_handle), std::move(client_ptr));

  if (IsSyncLoad()) {
    // Wait for OnCalculatedSize, which will also synchronously drain the data
    // pipe.
    binding_.WaitForIncomingMethodCall();
    if (received_on_complete_)
      return;
    if (!received_all_data_) {
      Failed(FileError::kNotReadableErr);
      return;
    }

    // Wait for OnComplete
    binding_.WaitForIncomingMethodCall();
    if (!received_on_complete_)
      Failed(FileError::kNotReadableErr);
  }
}

void FileReaderLoaderMojo::OnCalculatedSize(uint64_t total_size,
                                            uint64_t expected_content_size) {
  OnStartLoading(expected_content_size);
  expected_content_size_ = expected_content_size;
  if (expected_content_size_ == 0) {
    received_all_data_ = true;
    return;
  }

  if (IsSyncLoad()) {
    OnDataPipeReadable(MOJO_RESULT_OK);
  } else {
    handle_watcher_.Watch(
        consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        ConvertToBaseCallback(WTF::Bind(
            &FileReaderLoaderMojo::OnDataPipeReadable, WTF::Unretained(this))));
  }
}

void FileReaderLoaderMojo::OnComplete(int32_t status, uint64_t data_length) {
  if (status != net::OK ||
      data_length != static_cast<uint64_t>(expected_content_size_)) {
    Failed(status == net::ERR_FILE_NOT_FOUND ? FileError::kNotFoundErr
                                             : FileError::kNotReadableErr);
    return;
  }

  received_on_complete_ = true;
  if (received_all_data_)
    OnFinishLoading();
}

void FileReaderLoaderMojo::OnDataPipeReadable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    if (!received_all_data_)
      Failed(FileError::kNotReadableErr);
    return;
  }

  while (true) {
    uint32_t num_bytes;
    const void* buffer;
    MojoResult result = consumer_handle_->BeginReadData(
        &buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      if (!IsSyncLoad())
        return;

      result = mojo::Wait(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE);
      if (result == MOJO_RESULT_OK)
        continue;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Pipe closed.
      if (!received_all_data_)
        Failed(FileError::kNotReadableErr);
      return;
    }
    if (result != MOJO_RESULT_OK) {
      Failed(FileError::kNotReadableErr);
      return;
    }
    OnReceivedData(static_cast<const char*>(buffer), num_bytes);
    consumer_handle_->EndReadData(num_bytes);
    if (BytesLoaded() >= expected_content_size_) {
      received_all_data_ = true;
      if (received_on_complete_)
        OnFinishLoading();
      return;
    }
  }
}

}  // namespace

// static
std::unique_ptr<FileReaderLoader> FileReaderLoader::Create(
    ReadType read_type,
    FileReaderLoaderClient* client) {
  if (RuntimeEnabledFeatures::MojoBlobsEnabled())
    return std::make_unique<FileReaderLoaderMojo>(read_type, client);
  return std::make_unique<FileReaderLoaderIPC>(read_type, client);
}

FileReaderLoader::FileReaderLoader(ReadType read_type,
                                   FileReaderLoaderClient* client)
    : read_type_(read_type), client_(client) {}

FileReaderLoader::~FileReaderLoader() {
  Cleanup();
  UnadjustReportedMemoryUsageToV8();
}

void FileReaderLoader::Cancel() {
  error_code_ = FileError::kAbortErr;
  Cleanup();
}

void FileReaderLoader::Cleanup() {
  // If we get any error, we do not need to keep a buffer around.
  if (error_code_) {
    raw_data_.reset();
    string_result_ = "";
    is_raw_data_converted_ = true;
    decoder_.reset();
    array_buffer_result_ = nullptr;
    UnadjustReportedMemoryUsageToV8();
  }
}

void FileReaderLoader::OnStartLoading(long long total_bytes) {
  // A negative value means that the content length wasn't specified.
  total_bytes_ = total_bytes;

  long long initial_buffer_length = -1;

  if (total_bytes_ >= 0) {
    initial_buffer_length = total_bytes_;
  } else {
    // Nothing is known about the size of the resource. Normalize
    // m_totalBytes to -1 and initialize the buffer for receiving with the
    // default size.
    total_bytes_ = -1;
  }

  DCHECK(!raw_data_);

  if (read_type_ != kReadByClient) {
    // Check that we can cast to unsigned since we have to do
    // so to call ArrayBuffer's create function.
    // FIXME: Support reading more than the current size limit of ArrayBuffer.
    if (initial_buffer_length > std::numeric_limits<unsigned>::max()) {
      Failed(FileError::kNotReadableErr);
      return;
    }

    if (initial_buffer_length < 0)
      raw_data_ = std::make_unique<ArrayBufferBuilder>();
    else
      raw_data_ = WTF::WrapUnique(
          new ArrayBufferBuilder(static_cast<unsigned>(initial_buffer_length)));

    if (!raw_data_ || !raw_data_->IsValid()) {
      Failed(FileError::kNotReadableErr);
      return;
    }

    if (initial_buffer_length >= 0) {
      // Total size is known. Set m_rawData to ignore overflowed data.
      raw_data_->SetVariableCapacity(false);
    }
  }

  if (client_)
    client_->DidStartLoading();
}

void FileReaderLoader::OnReceivedData(const char* data, unsigned data_length) {
  DCHECK(data);

  // Bail out if we already encountered an error.
  if (error_code_)
    return;

  if (read_type_ == kReadByClient) {
    bytes_loaded_ += data_length;

    if (client_)
      client_->DidReceiveDataForClient(data, data_length);
    return;
  }

  unsigned bytes_appended = raw_data_->Append(data, data_length);
  if (!bytes_appended) {
    raw_data_.reset();
    bytes_loaded_ = 0;
    Failed(FileError::kNotReadableErr);
    return;
  }
  bytes_loaded_ += bytes_appended;
  is_raw_data_converted_ = false;
  AdjustReportedMemoryUsageToV8(bytes_appended);

  if (client_)
    client_->DidReceiveData();
}

void FileReaderLoader::OnFinishLoading() {
  if (read_type_ != kReadByClient && raw_data_) {
    raw_data_->ShrinkToFit();
    is_raw_data_converted_ = false;
  }

  if (total_bytes_ == -1) {
    // Update m_totalBytes only when in dynamic buffer grow mode.
    total_bytes_ = bytes_loaded_;
  }

  finished_loading_ = true;

  Cleanup();
  if (client_)
    client_->DidFinishLoading();
}

void FileReaderLoader::AdjustReportedMemoryUsageToV8(int64_t usage) {
  if (!usage)
    return;
  memory_usage_reported_to_v8_ += usage;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(usage);
  DCHECK_GE(memory_usage_reported_to_v8_, 0);
}

void FileReaderLoader::UnadjustReportedMemoryUsageToV8() {
  if (!memory_usage_reported_to_v8_)
    return;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_reported_to_v8_);
  memory_usage_reported_to_v8_ = 0;
}

void FileReaderLoader::Failed(FileError::ErrorCode error_code) {
  // If an error was already reported, don't report this error again.
  if (error_code_ != FileError::kOK)
    return;
  error_code_ = error_code;
  Cleanup();
  if (client_)
    client_->DidFail(error_code_);
}

DOMArrayBuffer* FileReaderLoader::ArrayBufferResult() {
  DCHECK_EQ(read_type_, kReadAsArrayBuffer);
  if (array_buffer_result_)
    return array_buffer_result_;

  // If the loading is not started or an error occurs, return an empty result.
  if (!raw_data_ || error_code_)
    return nullptr;

  DOMArrayBuffer* result = DOMArrayBuffer::Create(raw_data_->ToArrayBuffer());
  if (finished_loading_) {
    array_buffer_result_ = result;
    AdjustReportedMemoryUsageToV8(
        -1 * static_cast<int64_t>(raw_data_->ByteLength()));
    raw_data_.reset();
  }
  return result;
}

String FileReaderLoader::StringResult() {
  DCHECK_NE(read_type_, kReadAsArrayBuffer);
  DCHECK_NE(read_type_, kReadByClient);

  if (!raw_data_ || error_code_ || is_raw_data_converted_)
    return string_result_;

  switch (read_type_) {
    case kReadAsArrayBuffer:
      // No conversion is needed.
      return string_result_;
    case kReadAsBinaryString:
      SetStringResult(raw_data_->ToString());
      break;
    case kReadAsText:
      SetStringResult(ConvertToText());
      break;
    case kReadAsDataURL:
      // Partial data is not supported when reading as data URL.
      if (finished_loading_)
        SetStringResult(ConvertToDataURL());
      break;
    default:
      NOTREACHED();
  }

  if (finished_loading_) {
    DCHECK(is_raw_data_converted_);
    AdjustReportedMemoryUsageToV8(
        -1 * static_cast<int64_t>(raw_data_->ByteLength()));
    raw_data_.reset();
  }
  return string_result_;
}

void FileReaderLoader::SetStringResult(const String& result) {
  AdjustReportedMemoryUsageToV8(
      -1 * static_cast<int64_t>(string_result_.CharactersSizeInBytes()));
  is_raw_data_converted_ = true;
  string_result_ = result;
  AdjustReportedMemoryUsageToV8(string_result_.CharactersSizeInBytes());
}

String FileReaderLoader::ConvertToText() {
  if (!bytes_loaded_)
    return "";

  // Decode the data.
  // The File API spec says that we should use the supplied encoding if it is
  // valid. However, we choose to ignore this requirement in order to be
  // consistent with how WebKit decodes the web content: always has the BOM
  // override the provided encoding.
  // FIXME: consider supporting incremental decoding to improve the perf.
  StringBuilder builder;
  if (!decoder_) {
    decoder_ = TextResourceDecoder::Create(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        encoding_.IsValid() ? encoding_ : UTF8Encoding()));
  }
  builder.Append(decoder_->Decode(static_cast<const char*>(raw_data_->Data()),
                                  raw_data_->ByteLength()));

  if (finished_loading_)
    builder.Append(decoder_->Flush());

  return builder.ToString();
}

String FileReaderLoader::ConvertToDataURL() {
  StringBuilder builder;
  builder.Append("data:");

  if (!bytes_loaded_)
    return builder.ToString();

  builder.Append(data_type_);
  builder.Append(";base64,");

  Vector<char> out;
  Base64Encode(static_cast<const char*>(raw_data_->Data()),
               raw_data_->ByteLength(), out);
  out.push_back('\0');
  builder.Append(out.data());

  return builder.ToString();
}

void FileReaderLoader::SetEncoding(const String& encoding) {
  if (!encoding.IsEmpty())
    encoding_ = WTF::TextEncoding(encoding);
}

}  // namespace blink
