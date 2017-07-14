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

#ifndef Blob_h
#define Blob_h

#include "bindings/core/v8/ArrayBufferOrArrayBufferViewOrBlobOrUSVString.h"
#include "core/CoreExport.h"
#include "core/html/URLRegistry.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class BlobPropertyBag;
class ExceptionState;
class ExecutionContext;
class ScriptState;

class CORE_EXPORT Blob : public GarbageCollectedFinalized<Blob>,
                         public ScriptWrappable,
                         public URLRegistrable,
                         public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Blob* Create(ExecutionContext*, ExceptionState&) {
    return new Blob(BlobDataHandle::Create());
  }

  static Blob* Create(
      ExecutionContext*,
      const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>&,
      const BlobPropertyBag&,
      ExceptionState&);

  static Blob* Create(PassRefPtr<BlobDataHandle> blob_data_handle) {
    return new Blob(std::move(blob_data_handle));
  }

  static Blob* Create(const unsigned char* data,
                      size_t bytes,
                      const String& content_type);

  ~Blob() override;

  virtual unsigned long long size() const { return blob_data_handle_->size(); }
  virtual Blob* slice(long long start,
                      long long end,
                      const String& content_type,
                      ExceptionState&) const;

  // To allow ExceptionState to be passed in last, manually enumerate the
  // optional argument overloads.
  Blob* slice(ExceptionState& exception_state) const {
    return slice(0, std::numeric_limits<long long>::max(), String(),
                 exception_state);
  }
  Blob* slice(long long start, ExceptionState& exception_state) const {
    return slice(start, std::numeric_limits<long long>::max(), String(),
                 exception_state);
  }
  Blob* slice(long long start,
              long long end,
              ExceptionState& exception_state) const {
    return slice(start, end, String(), exception_state);
  }

  virtual void close(ScriptState*, ExceptionState&);

  String type() const { return blob_data_handle_->GetType(); }
  String Uuid() const { return blob_data_handle_->Uuid(); }
  PassRefPtr<BlobDataHandle> GetBlobDataHandle() const {
    return blob_data_handle_;
  }
  // True for all File instances, including the user-built ones.
  virtual bool IsFile() const { return false; }
  // Only true for File instances that are backed by platform files.
  virtual bool HasBackingFile() const { return false; }
  bool isClosed() const { return is_closed_; }

  // Used by the JavaScript Blob and File constructors.
  virtual void AppendTo(BlobData&) const;

  // URLRegistrable to support PublicURLs.
  URLRegistry& Registry() const final;

  // ImageBitmapSource implementation
  bool IsBlob() const override { return true; }

  DEFINE_INLINE_TRACE() {}

 protected:
  explicit Blob(PassRefPtr<BlobDataHandle>);

  static void PopulateBlobData(
      BlobData*,
      const HeapVector<ArrayBufferOrArrayBufferViewOrBlobOrUSVString>& parts,
      bool normalize_line_endings_to_native);
  static void ClampSliceOffsets(long long size,
                                long long& start,
                                long long& end);

  // Called by the Blob and File constructors when processing the 'type'
  // option per the FileAPI standard. Returns "" if |type| contains any
  // character outside U+0020...U+007E, or |type| ASCII-lowercased otherwise.
  static String NormalizeType(const String& type);

 private:
  Blob();

  RefPtr<BlobDataHandle> blob_data_handle_;
  bool is_closed_;
};

}  // namespace blink

#endif  // Blob_h
