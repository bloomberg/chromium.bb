/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/fileapi/Blob.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/BlobPropertyBag.h"
#include "core/frame/UseCounter.h"
#include "core/url/DOMURL.h"
#include "platform/bindings/ScriptState.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"

namespace blink {

namespace {

class BlobURLRegistry final : public URLRegistry {
 public:
  // SecurityOrigin is passed together with KURL so that the registry can
  // save it for entries from whose KURL the origin is not recoverable by
  // using BlobURL::getOrigin().
  void RegisterURL(SecurityOrigin*, const KURL&, URLRegistrable*) override;
  void UnregisterURL(const KURL&) override;

  static URLRegistry& Registry();
};

void BlobURLRegistry::RegisterURL(SecurityOrigin* origin,
                                  const KURL& public_url,
                                  URLRegistrable* registrable_object) {
  DCHECK_EQ(&registrable_object->Registry(), this);
  Blob* blob = static_cast<Blob*>(registrable_object);
  BlobRegistry::RegisterPublicBlobURL(origin, public_url,
                                      blob->GetBlobDataHandle());
}

void BlobURLRegistry::UnregisterURL(const KURL& public_url) {
  BlobRegistry::RevokePublicBlobURL(public_url);
}

URLRegistry& BlobURLRegistry::Registry() {
  // This is called on multiple threads.
  // (This code assumes it is safe to register or unregister URLs on
  // BlobURLRegistry (that is implemented by the embedder) on
  // multiple threads.)
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BlobURLRegistry, instance, ());
  return instance;
}

}  // namespace

Blob::Blob(PassRefPtr<BlobDataHandle> data_handle)
    : blob_data_handle_(std::move(data_handle)), is_closed_(false) {}

Blob::~Blob() {}

// static
Blob* Blob::Create(
    ExecutionContext* context,
    const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>& blob_parts,
    const BlobPropertyBag& options,
    ExceptionState& exception_state) {
  DCHECK(options.hasType());

  DCHECK(options.hasEndings());
  bool normalize_line_endings_to_native = options.endings() == "native";
  if (normalize_line_endings_to_native)
    UseCounter::Count(context, WebFeature::kFileAPINativeLineEndings);

  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->SetContentType(NormalizeType(options.type()));

  PopulateBlobData(blob_data.get(), blob_parts,
                   normalize_line_endings_to_native);

  long long blob_size = blob_data->length();
  return new Blob(BlobDataHandle::Create(std::move(blob_data), blob_size));
}

Blob* Blob::Create(const unsigned char* data,
                   size_t bytes,
                   const String& content_type) {
  DCHECK(data);

  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->SetContentType(content_type);
  blob_data->AppendBytes(data, bytes);
  long long blob_size = blob_data->length();

  return new Blob(BlobDataHandle::Create(std::move(blob_data), blob_size));
}

// static
void Blob::PopulateBlobData(
    BlobData* blob_data,
    const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>& parts,
    bool normalize_line_endings_to_native) {
  for (const auto& item : parts) {
    if (item.isArrayBuffer()) {
      DOMArrayBuffer* array_buffer = item.getAsArrayBuffer();
      blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
    } else if (item.isArrayBufferView()) {
      DOMArrayBufferView* array_buffer_view =
          item.getAsArrayBufferView().View();
      blob_data->AppendBytes(array_buffer_view->BaseAddress(),
                             array_buffer_view->byteLength());
    } else if (item.isBlob()) {
      item.getAsBlob()->AppendTo(*blob_data);
    } else if (item.isUSVString()) {
      blob_data->AppendText(item.getAsUSVString(),
                            normalize_line_endings_to_native);
    } else {
      NOTREACHED();
    }
  }
}

// static
void Blob::ClampSliceOffsets(long long size, long long& start, long long& end) {
  DCHECK_NE(size, -1);

  // Convert the negative value that is used to select from the end.
  if (start < 0)
    start = start + size;
  if (end < 0)
    end = end + size;

  // Clamp the range if it exceeds the size limit.
  if (start < 0)
    start = 0;
  if (end < 0)
    end = 0;
  if (start >= size) {
    start = 0;
    end = 0;
  } else if (end < start) {
    end = start;
  } else if (end > size) {
    end = size;
  }
}

Blob* Blob::slice(long long start,
                  long long end,
                  const String& content_type,
                  ExceptionState& exception_state) const {
  if (isClosed()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Blob has been closed.");
    return nullptr;
  }

  long long size = this->size();
  ClampSliceOffsets(size, start, end);

  long long length = end - start;
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->SetContentType(NormalizeType(content_type));
  blob_data->AppendBlob(blob_data_handle_, start, length);
  return Blob::Create(BlobDataHandle::Create(std::move(blob_data), length));
}

void Blob::close(ScriptState* script_state, ExceptionState& exception_state) {
  if (isClosed()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Blob has been closed.");
    return;
  }

  // Dereferencing a Blob that has been closed should result in
  // a network error. Revoke URLs registered against it through
  // its UUID.
  DOMURL::RevokeObjectUUID(ExecutionContext::From(script_state), Uuid());

  // A Blob enters a 'readability state' of closed, where it will report its
  // size as zero. Blob and FileReader operations now throws on
  // being passed a Blob in that state. Downstream uses of closed Blobs
  // (e.g., XHR.send()) consider them as empty.
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->SetContentType(type());
  blob_data_handle_ = BlobDataHandle::Create(std::move(blob_data), 0);
  is_closed_ = true;
}

void Blob::AppendTo(BlobData& blob_data) const {
  blob_data.AppendBlob(blob_data_handle_, 0, blob_data_handle_->size());
}

URLRegistry& Blob::Registry() const {
  return BlobURLRegistry::Registry();
}

// static
String Blob::NormalizeType(const String& type) {
  if (type.IsNull())
    return g_empty_string;
  const size_t length = type.length();
  if (type.Is8Bit()) {
    const LChar* chars = type.Characters8();
    for (size_t i = 0; i < length; ++i) {
      if (chars[i] < 0x20 || chars[i] > 0x7e)
        return g_empty_string;
    }
  } else {
    const UChar* chars = type.Characters16();
    for (size_t i = 0; i < length; ++i) {
      if (chars[i] < 0x0020 || chars[i] > 0x007e)
        return g_empty_string;
    }
  }
  return type.DeprecatedLower();
}

}  // namespace blink
