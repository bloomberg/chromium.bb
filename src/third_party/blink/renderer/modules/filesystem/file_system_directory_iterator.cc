// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_directory_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_base_handle.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_directory_iterator_entry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemDirectoryIterator::FileSystemDirectoryIterator(
    DOMFileSystemBase* file_system,
    const String& full_path)
    : DirectoryReaderBase(file_system, full_path) {
  auto success_callback_wrapper = WTF::BindRepeating(
      [](FileSystemDirectoryIterator* persistent_reader,
         EntryHeapVector* entries) { persistent_reader->AddEntries(*entries); },
      WrapPersistentIfNeeded(this));
  auto error_callback_wrapper = WTF::Bind(&FileSystemDirectoryIterator::OnError,
                                          WrapPersistentIfNeeded(this));

  Filesystem()->ReadDirectory(this, full_path_, success_callback_wrapper,
                              std::move(error_callback_wrapper));
}

ScriptPromise FileSystemDirectoryIterator::next(ScriptState* script_state) {
  if (error_ != base::File::FILE_OK) {
    return ScriptPromise::RejectWithDOMException(
        script_state, file_error::CreateDOMException(error_));
  }

  if (!entries_.IsEmpty()) {
    FileSystemDirectoryIteratorEntry* result =
        FileSystemDirectoryIteratorEntry::Create();
    result->setValue(entries_.TakeFirst()->asFileSystemHandle());
    return ScriptPromise::Cast(script_state, ToV8(result, script_state));
  }

  if (has_more_entries_) {
    DCHECK(!pending_next_);
    pending_next_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    return pending_next_->Promise();
  }

  FileSystemDirectoryIteratorEntry* result =
      FileSystemDirectoryIteratorEntry::Create();
  result->setDone(true);
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void FileSystemDirectoryIterator::Trace(Visitor* visitor) {
  DirectoryReaderBase::Trace(visitor);
  visitor->Trace(entries_);
  visitor->Trace(pending_next_);
}

void FileSystemDirectoryIterator::AddEntries(const EntryHeapVector& entries) {
  for (const auto& e : entries)
    entries_.emplace_back(e);
  if (pending_next_) {
    ScriptState::Scope scope(pending_next_->GetScriptState());
    pending_next_->Resolve(
        next(pending_next_->GetScriptState()).GetScriptValue());
    pending_next_ = nullptr;
  }
}

void FileSystemDirectoryIterator::OnError(base::File::Error error) {
  error_ = error;
  if (pending_next_) {
    pending_next_->Reject(file_error::CreateDOMException(error));
    pending_next_ = nullptr;
  }
}

}  // namespace blink
