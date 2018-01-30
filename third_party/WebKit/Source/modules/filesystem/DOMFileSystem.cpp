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

#include "modules/filesystem/DOMFileSystem.h"

#include <memory>

#include "core/probe/CoreProbes.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/FileWriter.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemCallbacks.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

namespace {

void RunCallback(ExecutionContext* execution_context,
                 base::OnceClosure task,
                 std::unique_ptr<int> identifier) {
  if (!execution_context)
    return;
  DCHECK(execution_context->IsContextThread());
  probe::AsyncTask async_task(execution_context, identifier.get());
  std::move(task).Run();
}

}  // namespace

// static
DOMFileSystem* DOMFileSystem::Create(ExecutionContext* context,
                                     const String& name,
                                     FileSystemType type,
                                     const KURL& root_url) {
  return new DOMFileSystem(context, name, type, root_url);
}

DOMFileSystem* DOMFileSystem::CreateIsolatedFileSystem(
    ExecutionContext* context,
    const String& filesystem_id) {
  if (filesystem_id.IsEmpty())
    return nullptr;

  StringBuilder filesystem_name;
  filesystem_name.Append(Platform::Current()->FileSystemCreateOriginIdentifier(
      WebSecurityOrigin(context->GetSecurityOrigin())));
  filesystem_name.Append(":Isolated_");
  filesystem_name.Append(filesystem_id);

  // The rootURL created here is going to be attached to each filesystem request
  // and is to be validated each time the request is being handled.
  StringBuilder root_url;
  root_url.Append("filesystem:");
  root_url.Append(context->GetSecurityOrigin()->ToString());
  root_url.Append('/');
  root_url.Append(kIsolatedPathPrefix);
  root_url.Append('/');
  root_url.Append(filesystem_id);
  root_url.Append('/');

  return DOMFileSystem::Create(context, filesystem_name.ToString(),
                               kFileSystemTypeIsolated,
                               KURL(root_url.ToString()));
}

DOMFileSystem::DOMFileSystem(ExecutionContext* context,
                             const String& name,
                             FileSystemType type,
                             const KURL& root_url)
    : DOMFileSystemBase(context, name, type, root_url),
      ContextClient(context),
      number_of_pending_callbacks_(0),
      root_entry_(DirectoryEntry::Create(this, DOMFilePath::kRoot)) {}

DirectoryEntry* DOMFileSystem::root() const {
  return root_entry_.Get();
}

void DOMFileSystem::AddPendingCallbacks() {
  ++number_of_pending_callbacks_;
}

void DOMFileSystem::RemovePendingCallbacks() {
  DCHECK_GT(number_of_pending_callbacks_, 0);
  --number_of_pending_callbacks_;
}

bool DOMFileSystem::HasPendingActivity() const {
  DCHECK_GE(number_of_pending_callbacks_, 0);
  return number_of_pending_callbacks_;
}

void DOMFileSystem::ReportError(ErrorCallbackBase* error_callback,
                                FileError::ErrorCode file_error) {
  ReportError(GetExecutionContext(), error_callback, file_error);
}

void DOMFileSystem::ReportError(ExecutionContext* execution_context,
                                ErrorCallbackBase* error_callback,
                                FileError::ErrorCode file_error) {
  if (!error_callback)
    return;
  ScheduleCallback(execution_context,
                   WTF::Bind(&ErrorCallbackBase::Invoke,
                             WrapPersistent(error_callback), file_error));
}

void DOMFileSystem::CreateWriter(
    const FileEntry* file_entry,
    FileWriterCallbacks::OnDidCreateFileWriterCallback* success_callback,
    ErrorCallbackBase* error_callback) {
  DCHECK(file_entry);

  if (!FileSystem()) {
    ReportError(error_callback, FileError::kAbortErr);
    return;
  }

  FileWriter* file_writer = FileWriter::Create(GetExecutionContext());
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks =
      FileWriterCallbacks::Create(file_writer, success_callback, error_callback,
                                  context_);
  FileSystem()->CreateFileWriter(CreateFileSystemURL(file_entry), file_writer,
                                 std::move(callbacks));
}

void DOMFileSystem::CreateFile(
    const FileEntry* file_entry,
    SnapshotFileCallback::OnDidCreateSnapshotFileCallback* success_callback,
    ErrorCallbackBase* error_callback) {
  KURL file_system_url = CreateFileSystemURL(file_entry);
  if (!FileSystem()) {
    ReportError(error_callback, FileError::kAbortErr);
    return;
  }

  FileSystem()->CreateSnapshotFileAndReadMetadata(
      file_system_url,
      SnapshotFileCallback::Create(this, file_entry->name(), file_system_url,
                                   success_callback, error_callback, context_));
}

void DOMFileSystem::ScheduleCallback(ExecutionContext* execution_context,
                                     base::OnceClosure task) {
  if (!execution_context)
    return;

  DCHECK(execution_context->IsContextThread());

  std::unique_ptr<int> identifier = std::make_unique<int>(0);
  probe::AsyncTaskScheduled(execution_context, TaskNameForInstrumentation(),
                            identifier.get());
  execution_context->GetTaskRunner(TaskType::kFileReading)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&RunCallback, WrapWeakPersistent(execution_context),
                           WTF::Passed(std::move(task)),
                           WTF::Passed(std::move(identifier))));
}

void DOMFileSystem::Trace(blink::Visitor* visitor) {
  visitor->Trace(root_entry_);
  DOMFileSystemBase::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
