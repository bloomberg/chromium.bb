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
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/ThreadableLoader.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebURLRequest.h"
#include "v8/include/v8.h"

namespace blink {

FileReaderLoader::FileReaderLoader(ReadType read_type,
                                   FileReaderLoaderClient* client)
    : read_type_(read_type), client_(client) {}

FileReaderLoader::~FileReaderLoader() {
  Cleanup();
  UnadjustReportedMemoryUsageToV8();
  if (!url_for_reading_.IsEmpty()) {
    BlobRegistry::RevokePublicBlobURL(url_for_reading_);
  }
}

void FileReaderLoader::Start(ExecutionContext* execution_context,
                             scoped_refptr<BlobDataHandle> blob_data) {
  DCHECK(execution_context);
  // The blob is read by routing through the request handling layer given a
  // temporary public url.
  url_for_reading_ =
      BlobURL::CreatePublicURL(execution_context->GetSecurityOrigin());
  if (url_for_reading_.IsEmpty()) {
    Failed(FileError::kSecurityErr);
    return;
  }

  BlobRegistry::RegisterPublicBlobURL(execution_context->GetSecurityOrigin(),
                                      url_for_reading_, std::move(blob_data));
  // Construct and load the request.
  ResourceRequest request(url_for_reading_);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context->GetSecurityContext().AddressSpace());

  // FIXME: Should this really be 'internal'? Do we know anything about the
  // actual request that generated this fetch?
  request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);

  request.SetHTTPMethod(HTTPNames::GET);

  ThreadableLoaderOptions options;

  ResourceLoaderOptions resource_loader_options;
  // Use special initiator to hide the request from the inspector.
  resource_loader_options.initiator_info.name =
      FetchInitiatorTypeNames::internal;

  if (client_) {
    DCHECK(!loader_);
    loader_ = ThreadableLoader::Create(*execution_context, this, options,
                                       resource_loader_options);
    loader_->Start(request);
  } else {
    ThreadableLoader::LoadResourceSynchronously(
        *execution_context, request, *this, options, resource_loader_options);
  }
}

void FileReaderLoader::Cancel() {
  error_code_ = FileError::kAbortErr;
  Cleanup();
}

void FileReaderLoader::Cleanup() {
  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }

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

void FileReaderLoader::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  if (response.HttpStatusCode() != 200) {
    Failed(HttpStatusCodeToErrorCode(response.HttpStatusCode()));
    return;
  }

  // A negative value means that the content length wasn't specified.
  total_bytes_ = response.ExpectedContentLength();

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
      raw_data_ = WTF::MakeUnique<ArrayBufferBuilder>();
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

void FileReaderLoader::DidReceiveData(const char* data, unsigned data_length) {
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

void FileReaderLoader::DidFinishLoading(unsigned long, double) {
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

void FileReaderLoader::DidFail(const ResourceError& error) {
  if (error.IsCancellation())
    return;
  // If we're aborting, do not proceed with normal error handling since it is
  // covered in aborting code.
  if (error_code_ == FileError::kAbortErr)
    return;

  Failed(FileError::kNotReadableErr);
}

void FileReaderLoader::Failed(FileError::ErrorCode error_code) {
  error_code_ = error_code;
  Cleanup();
  if (client_)
    client_->DidFail(error_code_);
}

FileError::ErrorCode FileReaderLoader::HttpStatusCodeToErrorCode(
    int http_status_code) {
  switch (http_status_code) {
    case 403:
      return FileError::kSecurityErr;
    case 404:
      return FileError::kNotFoundErr;
    default:
      return FileError::kNotReadableErr;
  }
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
