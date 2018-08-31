// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_directory_handle.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system_base.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"

namespace blink {

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    DOMFileSystemBase* file_system,
    const String& full_path)
    : FileSystemBaseHandle(file_system, full_path) {}

ScriptPromise FileSystemDirectoryHandle::getFile(ScriptState* script_state,
                                                 const String& name) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->GetFile(this, name, FileSystemFlags(),
                        new EntryCallbacks::OnDidGetEntryPromiseImpl(resolver),
                        new PromiseErrorCallback(resolver));
  return result;
}

ScriptPromise FileSystemDirectoryHandle::getDirectory(ScriptState* script_state,
                                                      const String& name) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->GetDirectory(
      this, name, FileSystemFlags(),
      new EntryCallbacks::OnDidGetEntryPromiseImpl(resolver),
      new PromiseErrorCallback(resolver));
  return result;
}

// static
ScriptPromise FileSystemDirectoryHandle::getSystemDirectory(
    ScriptState* script_state,
    const GetSystemDirectoryOptions& options) {
  auto* context = ExecutionContext::From(script_state);

  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();

  LocalFileSystem::From(*context)->RequestFileSystem(
      context, mojom::blink::FileSystemType::kTemporary, /*size=*/0,
      FileSystemCallbacks::Create(
          new FileSystemCallbacks::OnDidOpenFileSystemPromiseImpl(resolver),
          new PromiseErrorCallback(resolver), context,
          mojom::blink::FileSystemType::kTemporary));
  return result;
}

}  // namespace blink
