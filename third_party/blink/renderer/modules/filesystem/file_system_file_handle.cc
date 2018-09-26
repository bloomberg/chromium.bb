// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_file_handle.h"

#include "third_party/blink/public/mojom/filesystem/file_writer.mojom-blink.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_writer.h"

namespace blink {

namespace {

class CreateWriterCallbacks
    : public WebCallbacks<mojom::blink::FileWriterPtr, base::File::Error> {
 public:
  explicit CreateWriterCallbacks(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(mojom::blink::FileWriterPtr writer) override {
    resolver_->Resolve(new FileSystemWriter(std::move(writer)));
  }

  void OnError(base::File::Error error) override {
    resolver_->Reject(FileError::CreateDOMException(error));
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
};

class OnDidCreateSnapshotFilePromise
    : public SnapshotFileCallback::OnDidCreateSnapshotFileCallback {
 public:
  explicit OnDidCreateSnapshotFilePromise(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  void Trace(Visitor* visitor) override {
    OnDidCreateSnapshotFileCallback::Trace(visitor);
    visitor->Trace(resolver_);
  }
  void OnSuccess(File* file) override { resolver_->Resolve(file); }

 private:
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace

FileSystemFileHandle::FileSystemFileHandle(DOMFileSystemBase* file_system,
                                           const String& full_path)
    : FileSystemBaseHandle(file_system, full_path) {}

ScriptPromise FileSystemFileHandle::createWriter(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  FileSystemDispatcher::GetThreadSpecificInstance().CreateFileWriter(
      filesystem()->CreateFileSystemURL(this),
      std::make_unique<CreateWriterCallbacks>(resolver));
  return result;
}

ScriptPromise FileSystemFileHandle::getFile(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  KURL file_system_url = filesystem()->CreateFileSystemURL(this);
  FileSystemDispatcher::GetThreadSpecificInstance().CreateSnapshotFile(
      file_system_url,
      SnapshotFileCallback::Create(filesystem(), name(), file_system_url,
                                   new OnDidCreateSnapshotFilePromise(resolver),
                                   new PromiseErrorCallback(resolver),
                                   ExecutionContext::From(script_state)));
  return result;
}

}  // namespace blink
