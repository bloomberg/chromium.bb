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
#include "modules/filesystem/Entry.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/fileapi/FileError.h"
#include "core/frame/UseCounter.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

Entry::Entry(DOMFileSystemBase* fileSystem, const String& fullPath)
    : EntryBase(fileSystem, fullPath) {}

DOMFileSystem* Entry::filesystem(ScriptState* scriptState) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(
        scriptState->getExecutionContext(),
        UseCounter::Entry_Filesystem_AttributeGetter_IsolatedFileSystem);
  return filesystem();
}

void Entry::getMetadata(ScriptState* scriptState,
                        MetadataCallback* successCallback,
                        ErrorCallback* errorCallback) {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_GetMetadata_Method_IsolatedFileSystem);
  m_fileSystem->getMetadata(this, successCallback,
                            ScriptErrorCallback::wrap(errorCallback));
}

void Entry::moveTo(ScriptState* scriptState,
                   DirectoryEntry* parent,
                   const String& name,
                   EntryCallback* successCallback,
                   ErrorCallback* errorCallback) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_MoveTo_Method_IsolatedFileSystem);
  m_fileSystem->move(this, parent, name, successCallback,
                     ScriptErrorCallback::wrap(errorCallback));
}

void Entry::copyTo(ScriptState* scriptState,
                   DirectoryEntry* parent,
                   const String& name,
                   EntryCallback* successCallback,
                   ErrorCallback* errorCallback) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_CopyTo_Method_IsolatedFileSystem);
  m_fileSystem->copy(this, parent, name, successCallback,
                     ScriptErrorCallback::wrap(errorCallback));
}

void Entry::remove(ScriptState* scriptState,
                   VoidCallback* successCallback,
                   ErrorCallback* errorCallback) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_Remove_Method_IsolatedFileSystem);
  m_fileSystem->remove(this, successCallback,
                       ScriptErrorCallback::wrap(errorCallback));
}

void Entry::getParent(ScriptState* scriptState,
                      EntryCallback* successCallback,
                      ErrorCallback* errorCallback) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_GetParent_Method_IsolatedFileSystem);
  m_fileSystem->getParent(this, successCallback,
                          ScriptErrorCallback::wrap(errorCallback));
}

String Entry::toURL(ScriptState* scriptState) const {
  if (m_fileSystem->type() == FileSystemTypeIsolated)
    UseCounter::count(scriptState->getExecutionContext(),
                      UseCounter::Entry_ToURL_Method_IsolatedFileSystem);
  return static_cast<const EntryBase*>(this)->toURL();
}

DEFINE_TRACE(Entry) {
  EntryBase::trace(visitor);
}

}  // namespace blink
