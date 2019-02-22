// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_base_handle.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system_base.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_directory_handle.h"

namespace blink {

FileSystemBaseHandle::FileSystemBaseHandle(DOMFileSystemBase* file_system,
                                           const String& full_path)
    : EntryBase(file_system, full_path) {}

ScriptPromise FileSystemBaseHandle::getParent(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->GetParent(
      this, new EntryCallbacks::OnDidGetEntryPromiseImpl(resolver),
      new PromiseErrorCallback(resolver));
  return result;
}

ScriptPromise FileSystemBaseHandle::moveTo(ScriptState* script_state,
                                           FileSystemDirectoryHandle* parent,
                                           const String& name) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->Move(this, parent, name,
                     new EntryCallbacks::OnDidGetEntryPromiseImpl(resolver),
                     new PromiseErrorCallback(resolver));
  return result;
}

ScriptPromise FileSystemBaseHandle::copyTo(ScriptState* script_state,
                                           FileSystemDirectoryHandle* parent,
                                           const String& name) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->Copy(this, parent, name,
                     new EntryCallbacks::OnDidGetEntryPromiseImpl(resolver),
                     new PromiseErrorCallback(resolver));
  return result;
}

ScriptPromise FileSystemBaseHandle::remove(ScriptState* script_state) {
  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  filesystem()->Remove(this,
                       new VoidCallbacks::OnDidSucceedPromiseImpl(resolver),
                       new PromiseErrorCallback(resolver));
  return result;
}

}  // namespace blink
