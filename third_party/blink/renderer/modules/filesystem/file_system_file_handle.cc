// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_file_handle.h"

#include "third_party/blink/public/mojom/filesystem/file_writer.mojom-blink.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_writer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemFileHandle::FileSystemFileHandle(DOMFileSystemBase* file_system,
                                           const String& full_path)
    : FileSystemBaseHandle(file_system, full_path) {}

ScriptPromise FileSystemFileHandle::createWriter(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();
  FileSystemDispatcher::From(ExecutionContext::From(script_state))
      .GetFileSystemManager()
      .CreateWriter(
          filesystem()->CreateFileSystemURL(this),
          WTF::Bind(
              [](ScriptPromiseResolver* resolver, base::File::Error result,
                 mojom::blink::FileWriterPtr writer) {
                if (result == base::File::FILE_OK) {
                  resolver->Resolve(MakeGarbageCollected<FileSystemWriter>(
                      std::move(writer)));
                } else {
                  resolver->Reject(file_error::CreateDOMException(result));
                }
              },
              WrapPersistent(resolver)));
  return result;
}

ScriptPromise FileSystemFileHandle::getFile(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();
  KURL file_system_url = filesystem()->CreateFileSystemURL(this);

  auto success_callback_wrapper =
      WTF::Bind([](ScriptPromiseResolver* resolver,
                   File* file) { resolver->Resolve(file); },
                WrapPersistentIfNeeded(resolver));
  auto error_callback_wrapper = AsyncCallbackHelper::ErrorPromise(resolver);

  FileSystemDispatcher::From(ExecutionContext::From(script_state))
      .CreateSnapshotFile(file_system_url,
                          std::make_unique<SnapshotFileCallback>(
                              filesystem(), name(), file_system_url,
                              std::move(success_callback_wrapper),
                              std::move(error_callback_wrapper),
                              ExecutionContext::From(script_state)));
  return result;
}

}  // namespace blink
