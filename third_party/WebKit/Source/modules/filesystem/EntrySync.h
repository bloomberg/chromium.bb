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

#ifndef EntrySync_h
#define EntrySync_h

#include "modules/filesystem/DOMFileSystemSync.h"
#include "modules/filesystem/EntryBase.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class DirectoryEntrySync;
class Metadata;
class ExceptionState;

class EntrySync : public EntryBase, public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EntrySync* Create(EntryBase*);

  DOMFileSystemSync* filesystem() const {
    return static_cast<DOMFileSystemSync*>(file_system_.Get());
  }

  Metadata* getMetadata(ExceptionState&);
  EntrySync* moveTo(DirectoryEntrySync* parent,
                    const String& name,
                    ExceptionState&) const;
  EntrySync* copyTo(DirectoryEntrySync* parent,
                    const String& name,
                    ExceptionState&) const;
  void remove(ExceptionState&) const;
  EntrySync* getParent() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  EntrySync(DOMFileSystemBase*, const String& full_path);
};

}  // namespace blink

#endif  // EntrySync_h
