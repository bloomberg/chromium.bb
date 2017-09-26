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

#ifndef FileReaderLoader_h
#define FileReaderLoader_h

#include <memory>
#include "core/CoreExport.h"
#include "core/fileapi/FileError.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBufferBuilder.h"

namespace blink {

class BlobDataHandle;
class DOMArrayBuffer;
class ExecutionContext;
class FileReaderLoaderClient;
class TextResourceDecoder;
class ThreadableLoader;

class CORE_EXPORT FileReaderLoader final : public ThreadableLoaderClient {
  USING_FAST_MALLOC(FileReaderLoader);

 public:
  enum ReadType {
    kReadAsArrayBuffer,
    kReadAsBinaryString,
    kReadAsText,
    kReadAsDataURL,
    kReadByClient
  };

  // If client is given, do the loading asynchronously. Otherwise, load
  // synchronously.
  static std::unique_ptr<FileReaderLoader> Create(
      ReadType read_type,
      FileReaderLoaderClient* client) {
    return WTF::WrapUnique(new FileReaderLoader(read_type, client));
  }

  ~FileReaderLoader() override;

  void Start(ExecutionContext*, RefPtr<BlobDataHandle>);
  void Cancel();

  // ThreadableLoaderClient
  void DidReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void DidReceiveData(const char*, unsigned) override;
  void DidFinishLoading(unsigned long, double) override;
  void DidFail(const ResourceError&) override;

  DOMArrayBuffer* ArrayBufferResult();
  String StringResult();

  // Returns the total bytes received. Bytes ignored by m_rawData won't be
  // counted.
  //
  // This value doesn't grow more than numeric_limits<unsigned> when
  // m_readType is not set to ReadByClient.
  long long BytesLoaded() const { return bytes_loaded_; }

  // Before didReceiveResponse() is called: Returns -1.
  // After didReceiveResponse() is called:
  // - If the size of the resource is known (from
  //   m_response.expectedContentLength() or once didFinishLoading() is
  //   called), returns it.
  // - Otherwise, returns -1.
  long long TotalBytes() const { return total_bytes_; }

  FileError::ErrorCode GetErrorCode() const { return error_code_; }

  void SetEncoding(const String&);
  void SetDataType(const String& data_type) { data_type_ = data_type; }

  bool HasFinishedLoading() const { return finished_loading_; }

 private:
  FileReaderLoader(ReadType, FileReaderLoaderClient*);

  void Cleanup();
  void AdjustReportedMemoryUsageToV8(int64_t usage);
  void UnadjustReportedMemoryUsageToV8();

  void Failed(FileError::ErrorCode);
  String ConvertToText();
  String ConvertToDataURL();
  void SetStringResult(const String&);

  static FileError::ErrorCode HttpStatusCodeToErrorCode(int);

  ReadType read_type_;
  FileReaderLoaderClient* client_;
  WTF::TextEncoding encoding_;
  String data_type_;

  KURL url_for_reading_;
  Persistent<ThreadableLoader> loader_;

  std::unique_ptr<ArrayBufferBuilder> raw_data_;
  bool is_raw_data_converted_ = false;

  Persistent<DOMArrayBuffer> array_buffer_result_;
  String string_result_;

  // The decoder used to decode the text data.
  std::unique_ptr<TextResourceDecoder> decoder_;

  bool finished_loading_ = false;
  long long bytes_loaded_ = 0;
  // If the total size of the resource is unknown, m_totalBytes is set to -1
  // until completion of loading, and the buffer for receiving data is set to
  // dynamically grow. Otherwise, m_totalBytes is set to the total size and
  // the buffer for receiving data of m_totalBytes is allocated and never grow
  // even when extra data is appeneded.
  long long total_bytes_ = -1;
  int64_t memory_usage_reported_to_v8_ = 0;

  FileError::ErrorCode error_code_ = FileError::kOK;
};

}  // namespace blink

#endif  // FileReaderLoader_h
