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

#ifndef DirectoryEntry_h
#define DirectoryEntry_h

#include "modules/ModulesExport.h"
#include "modules/filesystem/Entry.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMFileSystemBase;
class DirectoryReader;
class EntryCallback;
class ErrorCallback;
class FileSystemFlags;
class VoidCallback;

class MODULES_EXPORT DirectoryEntry final : public Entry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DirectoryEntry* Create(DOMFileSystemBase* file_system,
                                const String& full_path) {
    return new DirectoryEntry(file_system, full_path);
  }
  bool isDirectory() const override { return true; }

  DirectoryReader* createReader();
  void getFile(const String& path,
               const FileSystemFlags&,
               EntryCallback* = nullptr,
               ErrorCallback* = nullptr);
  void getDirectory(const String& path,
                    const FileSystemFlags&,
                    EntryCallback* = nullptr,
                    ErrorCallback* = nullptr);
  void removeRecursively(VoidCallback* success_callback = nullptr,
                         ErrorCallback* = nullptr) const;

  void Trace(blink::Visitor*) override;

 private:
  DirectoryEntry(DOMFileSystemBase*, const String& full_path);
};

DEFINE_TYPE_CASTS(DirectoryEntry,
                  Entry,
                  entry,
                  entry->isDirectory(),
                  entry.isDirectory());

}  // namespace blink

#endif  // DirectoryEntry_h
