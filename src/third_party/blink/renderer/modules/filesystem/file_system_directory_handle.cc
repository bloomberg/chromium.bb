// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_directory_handle.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system_base.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_directory_iterator.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"

namespace blink {

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    DOMFileSystemBase* file_system,
    const String& full_path)
    : FileSystemBaseHandle(file_system, full_path) {}

ScriptPromise FileSystemDirectoryHandle::getFile(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetFileOptions* options) {
  FileSystemFlags* flags = FileSystemFlags::Create();
  flags->setCreateFlag(options->create());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessPromise<Entry>(resolver);
  auto error_callback_wrapper = AsyncCallbackHelper::ErrorPromise(resolver);

  filesystem()->GetFile(this, name, flags, std::move(success_callback_wrapper),
                        std::move(error_callback_wrapper));
  return result;
}

ScriptPromise FileSystemDirectoryHandle::getDirectory(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetDirectoryOptions* options) {
  FileSystemFlags* flags = FileSystemFlags::Create();
  flags->setCreateFlag(options->create());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessPromise<Entry>(resolver);
  auto error_callback_wrapper = AsyncCallbackHelper::ErrorPromise(resolver);

  filesystem()->GetDirectory(this, name, flags,
                             std::move(success_callback_wrapper),
                             std::move(error_callback_wrapper));
  return result;
}

// static
ScriptPromise FileSystemDirectoryHandle::getSystemDirectory(
    ScriptState* script_state,
    const GetSystemDirectoryOptions* options) {
  auto* context = ExecutionContext::From(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessPromise<DOMFileSystem>(resolver);
  auto error_callback_wrapper = AsyncCallbackHelper::ErrorPromise(resolver);

  LocalFileSystem::From(*context)->RequestFileSystem(
      context, mojom::blink::FileSystemType::kTemporary, /*size=*/0,
      std::make_unique<FileSystemCallbacks>(
          std::move(success_callback_wrapper),
          std::move(error_callback_wrapper), context,
          mojom::blink::FileSystemType::kTemporary),
      LocalFileSystem::kAsynchronous);
  return result;
}

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

ScriptValue FileSystemDirectoryHandle::getEntries(ScriptState* script_state) {
  auto* iterator = MakeGarbageCollected<FileSystemDirectoryIterator>(
      filesystem(), fullPath());
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state, result);
}

ScriptPromise FileSystemDirectoryHandle::removeRecursively(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  auto success_callback_wrapper =
      AsyncCallbackHelper::VoidSuccessPromise(resolver);
  auto error_callback_wrapper = AsyncCallbackHelper::ErrorPromise(resolver);

  filesystem()->RemoveRecursively(this, std::move(success_callback_wrapper),
                                  std::move(error_callback_wrapper));
  return result;
}

}  // namespace blink
