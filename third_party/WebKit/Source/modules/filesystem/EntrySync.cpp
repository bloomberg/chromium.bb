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

#include "modules/filesystem/EntrySync.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/Metadata.h"
#include "modules/filesystem/SyncCallbackHelper.h"

namespace blink {

EntrySync* EntrySync::Create(EntryBase* entry) {
  if (entry->isFile())
    return FileEntrySync::Create(entry->file_system_, entry->full_path_);
  return DirectoryEntrySync::Create(entry->file_system_, entry->full_path_);
}

Metadata* EntrySync::getMetadata(ExceptionState& exception_state) {
  MetadataSyncCallbackHelper* helper = MetadataSyncCallbackHelper::Create();
  file_system_->GetMetadata(this, helper->GetSuccessCallback(),
                            helper->GetErrorCallback(),
                            DOMFileSystemBase::kSynchronous);
  return helper->GetResult(exception_state);
}

EntrySync* EntrySync::moveTo(DirectoryEntrySync* parent,
                             const String& name,
                             ExceptionState& exception_state) const {
  EntrySyncCallbackHelper* helper = EntrySyncCallbackHelper::Create();
  file_system_->Move(this, parent, name, helper->GetSuccessCallback(),
                     helper->GetErrorCallback(),
                     DOMFileSystemBase::kSynchronous);
  return helper->GetResult(exception_state);
}

EntrySync* EntrySync::copyTo(DirectoryEntrySync* parent,
                             const String& name,
                             ExceptionState& exception_state) const {
  EntrySyncCallbackHelper* helper = EntrySyncCallbackHelper::Create();
  file_system_->Copy(this, parent, name, helper->GetSuccessCallback(),
                     helper->GetErrorCallback(),
                     DOMFileSystemBase::kSynchronous);
  return helper->GetResult(exception_state);
}

void EntrySync::remove(ExceptionState& exception_state) const {
  VoidSyncCallbackHelper* helper = VoidSyncCallbackHelper::Create();
  file_system_->Remove(this, helper->GetSuccessCallback(),
                       helper->GetErrorCallback(),
                       DOMFileSystemBase::kSynchronous);
  helper->GetResult(exception_state);
}

EntrySync* EntrySync::getParent() const {
  // Sync verion of getParent doesn't throw exceptions.
  String parent_path = DOMFilePath::GetDirectory(fullPath());
  return DirectoryEntrySync::Create(file_system_, parent_path);
}

EntrySync::EntrySync(DOMFileSystemBase* file_system, const String& full_path)
    : EntryBase(file_system, full_path) {}

void EntrySync::Trace(blink::Visitor* visitor) {
  EntryBase::Trace(visitor);
}

}  // namespace blink
