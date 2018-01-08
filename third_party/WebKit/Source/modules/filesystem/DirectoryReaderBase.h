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

#ifndef DirectoryReaderBase_h
#define DirectoryReaderBase_h

#include "modules/filesystem/EntryHeapVector.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMFileSystemBase;

class DirectoryReaderBase : public ScriptWrappable {
 public:
  DOMFileSystemBase* Filesystem() const { return file_system_.Get(); }
  void SetHasMoreEntries(bool has_more_entries) {
    has_more_entries_ = has_more_entries;
  }

  virtual ~DirectoryReaderBase() = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(file_system_);
    ScriptWrappable::Trace(visitor);
  }

 protected:
  DirectoryReaderBase(DOMFileSystemBase* file_system, const String& full_path)
      : file_system_(file_system),
        full_path_(full_path),
        has_more_entries_(true) {}

  Member<DOMFileSystemBase> file_system_;

  // This is a virtual path.
  String full_path_;

  bool has_more_entries_;
};

class DirectoryReaderOnDidReadCallback
    : public GarbageCollectedFinalized<DirectoryReaderOnDidReadCallback> {
 public:
  virtual ~DirectoryReaderOnDidReadCallback() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void OnDidReadDirectoryEntries(const EntryHeapVector&) = 0;

 protected:
  DirectoryReaderOnDidReadCallback() = default;
};

}  // namespace blink

#endif  // DirectoryReaderBase_h
