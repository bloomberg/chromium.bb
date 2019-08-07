// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_

#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Blob;
class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class NativeFileSystemFileHandle;

class NativeFileSystemWritableFileStream final : public WritableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NativeFileSystemWritableFileStream(NativeFileSystemFileHandle*);

  void Trace(Visitor* visitor) override;

  // IDL defined functions for WritableStream.
  bool locked(ScriptState*, ExceptionState&) const override;
  ScriptPromise abort(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;
  ScriptValue getWriter(ScriptState*, ExceptionState&) override;
  void Serialize(ScriptState*, MessagePort* port, ExceptionState&) override;
  base::Optional<bool> IsLocked(ScriptState*, ExceptionState&) const override;

  // IDL defined functions specific to NativeFileSystemWritableFileStream.
  ScriptPromise write(ScriptState*,
                      uint64_t position,
                      const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
                      ExceptionState&);
  ScriptPromise truncate(ScriptState*, uint64_t size);

 private:
  ScriptPromise WriteBlob(ScriptState*, uint64_t position, Blob*);

  void WriteComplete(mojom::blink::NativeFileSystemErrorPtr result,
                     uint64_t bytes_written);
  void TruncateComplete(mojom::blink::NativeFileSystemErrorPtr result);

  Member<NativeFileSystemFileHandle> file_;

  Member<ScriptPromiseResolver> pending_operation_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
