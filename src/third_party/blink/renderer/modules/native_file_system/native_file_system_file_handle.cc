// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"

#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_writable_file_stream.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_writer.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemFileHandle::NativeFileSystemFileHandle(
    const String& name,
    mojom::blink::NativeFileSystemFileHandlePtr mojo_ptr)
    : NativeFileSystemHandle(name), mojo_ptr_(std::move(mojo_ptr)) {
  DCHECK(mojo_ptr_);
}

ScriptPromise NativeFileSystemFileHandle::createWriter(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  resolver->Resolve(MakeGarbageCollected<NativeFileSystemWriter>(this));
  return result;
}

ScriptPromise NativeFileSystemFileHandle::createWritable(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  resolver->Resolve(
      MakeGarbageCollected<NativeFileSystemWritableFileStream>(this));
  return result;
}

ScriptPromise NativeFileSystemFileHandle::getFile(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->AsBlob(WTF::Bind(
      [](ScriptPromiseResolver* resolver, const String& name,
         NativeFileSystemErrorPtr result,
         const scoped_refptr<BlobDataHandle>& blob) {
        if (result->error_code == base::File::FILE_OK) {
          resolver->Resolve(File::Create(name, InvalidFileTime(), blob));
        } else {
          resolver->Reject(file_error::CreateDOMException(result->error_code));
        }
      },
      WrapPersistent(resolver), name()));

  return result;
}

mojom::blink::NativeFileSystemTransferTokenPtr
NativeFileSystemFileHandle::Transfer() {
  mojom::blink::NativeFileSystemTransferTokenPtr result;
  mojo_ptr_->Transfer(mojo::MakeRequest(&result));
  return result;
}
void NativeFileSystemFileHandle::RemoveImpl(
    base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr)> callback) {
  mojo_ptr_->Remove(std::move(callback));
}

}  // namespace blink
