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

#include "modules/filesystem/DirectoryReader.h"

#include "core/fileapi/FileError.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"

namespace blink {

namespace {

void RunEntriesCallback(EntriesCallback* callback,
                        HeapVector<Member<Entry>>* entries) {
  callback->handleEvent(*entries);
}

}  // namespace

class DirectoryReader::EntriesCallbackHelper final : public EntriesCallback {
 public:
  explicit EntriesCallbackHelper(DirectoryReader* reader) : reader_(reader) {}

  void handleEvent(const EntryHeapVector& entries) override {
    reader_->AddEntries(entries);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(reader_);
    EntriesCallback::Trace(visitor);
  }

 private:
  // FIXME: This Member keeps the reader alive until all of the readDirectory
  // results are received. crbug.com/350285
  Member<DirectoryReader> reader_;
};

class DirectoryReader::ErrorCallbackHelper final : public ErrorCallbackBase {
 public:
  explicit ErrorCallbackHelper(DirectoryReader* reader) : reader_(reader) {}

  void Invoke(FileError::ErrorCode error) override { reader_->OnError(error); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(reader_);
    ErrorCallbackBase::Trace(visitor);
  }

 private:
  Member<DirectoryReader> reader_;
};

DirectoryReader::DirectoryReader(DOMFileSystemBase* file_system,
                                 const String& full_path)
    : DirectoryReaderBase(file_system, full_path), is_reading_(false) {}

DirectoryReader::~DirectoryReader() {}

void DirectoryReader::readEntries(EntriesCallback* entries_callback,
                                  ErrorCallback* error_callback) {
  if (!is_reading_) {
    is_reading_ = true;
    Filesystem()->ReadDirectory(this, full_path_,
                                new EntriesCallbackHelper(this),
                                new ErrorCallbackHelper(this));
  }

  if (error_) {
    Filesystem()->ReportError(ScriptErrorCallback::Wrap(error_callback),
                              error_);
    return;
  }

  if (entries_callback_) {
    // Non-null m_entriesCallback means multiple readEntries() calls are made
    // concurrently. We don't allow doing it.
    Filesystem()->ReportError(ScriptErrorCallback::Wrap(error_callback),
                              FileError::kInvalidStateErr);
    return;
  }

  if (!has_more_entries_ || !entries_.IsEmpty()) {
    if (entries_callback) {
      auto entries = new HeapVector<Member<Entry>>(std::move(entries_));
      DOMFileSystem::ScheduleCallback(
          Filesystem()->GetExecutionContext(),
          WTF::Bind(&RunEntriesCallback, WrapPersistent(entries_callback),
                    WrapPersistent(entries)));
    }
    entries_.clear();
    return;
  }

  entries_callback_ = entries_callback;
  error_callback_ = error_callback;
}

void DirectoryReader::AddEntries(const EntryHeapVector& entries) {
  entries_.AppendVector(entries);
  error_callback_ = nullptr;
  if (entries_callback_) {
    EntriesCallback* entries_callback = entries_callback_.Release();
    EntryHeapVector entries;
    entries.swap(entries_);
    entries_callback->handleEvent(entries);
  }
}

void DirectoryReader::OnError(FileError::ErrorCode error) {
  error_ = error;
  entries_callback_ = nullptr;
  if (error_callback_)
    error_callback_->handleEvent(FileError::CreateDOMException(error));
}

void DirectoryReader::Trace(blink::Visitor* visitor) {
  visitor->Trace(entries_);
  visitor->Trace(entries_callback_);
  visitor->Trace(error_callback_);
  DirectoryReaderBase::Trace(visitor);
}

}  // namespace blink
