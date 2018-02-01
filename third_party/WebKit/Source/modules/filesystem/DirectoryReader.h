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

#ifndef DirectoryReader_h
#define DirectoryReader_h

#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DirectoryReaderBase.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class V8EntriesCallback;
class V8ErrorCallback;

class DirectoryReader : public DirectoryReaderBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DirectoryReader* Create(DOMFileSystemBase* file_system,
                                 const String& full_path) {
    return new DirectoryReader(file_system, full_path);
  }

  ~DirectoryReader() override = default;

  void readEntries(V8EntriesCallback*, V8ErrorCallback* = nullptr);

  DOMFileSystem* Filesystem() const {
    return static_cast<DOMFileSystem*>(file_system_.Get());
  }

  void Trace(blink::Visitor*) override;

 private:
  class EntriesCallbackHelper;
  class ErrorCallbackHelper;

  DirectoryReader(DOMFileSystemBase*, const String& full_path);

  void AddEntries(const EntryHeapVector& entries);

  void OnError(FileError::ErrorCode);

  bool is_reading_;
  EntryHeapVector entries_;
  FileError::ErrorCode error_ = FileError::ErrorCode::kOK;
  Member<V8EntriesCallback> entries_callback_;
  Member<V8ErrorCallback> error_callback_;
};

}  // namespace blink

#endif  // DirectoryReader_h
