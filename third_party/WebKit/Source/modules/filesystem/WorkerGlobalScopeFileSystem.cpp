/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "modules/filesystem/WorkerGlobalScopeFileSystem.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/FileError.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/LocalFileSystem.h"
#include "modules/filesystem/SyncCallbackHelper.h"
#include "platform/FileSystemType.h"
#include "platform/weborigin/SecurityOrigin.h"
#include <memory>

namespace blink {

void WorkerGlobalScopeFileSystem::webkitRequestFileSystem(
    WorkerGlobalScope& worker,
    int type,
    long long size,
    FileSystemCallback* success_callback,
    ErrorCallback* error_callback) {
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem()) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kSecurityErr);
    return;
  }

  FileSystemType file_system_type = static_cast<FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kInvalidModificationErr);
    return;
  }

  LocalFileSystem::From(worker)->RequestFileSystem(
      &worker, file_system_type, size,
      FileSystemCallbacks::Create(success_callback,
                                  ScriptErrorCallback::Wrap(error_callback),
                                  &worker, file_system_type));
}

DOMFileSystemSync* WorkerGlobalScopeFileSystem::webkitRequestFileSystemSync(
    WorkerGlobalScope& worker,
    int type,
    long long size,
    ExceptionState& exception_state) {
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem()) {
    exception_state.ThrowSecurityError(FileError::kSecurityErrorMessage);
    return nullptr;
  }

  FileSystemType file_system_type = static_cast<FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    exception_state.ThrowDOMException(
        kInvalidModificationError,
        "the type must be kTemporary or kPersistent.");
    return nullptr;
  }

  FileSystemSyncCallbackHelper* helper = FileSystemSyncCallbackHelper::Create();
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks =
      FileSystemCallbacks::Create(helper->GetSuccessCallback(),
                                  helper->GetErrorCallback(), &worker,
                                  file_system_type);
  callbacks->SetShouldBlockUntilCompletion(true);

  LocalFileSystem::From(worker)->RequestFileSystem(&worker, file_system_type,
                                                   size, std::move(callbacks));
  return helper->GetResult(exception_state);
}

void WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemURL(
    WorkerGlobalScope& worker,
    const String& url,
    EntryCallback* success_callback,
    ErrorCallback* error_callback) {
  KURL completed_url = worker.CompleteURL(url);
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem() ||
      !secure_context->GetSecurityOrigin()->CanRequest(completed_url)) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kSecurityErr);
    return;
  }

  if (!completed_url.IsValid()) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kEncodingErr);
    return;
  }

  LocalFileSystem::From(worker)->ResolveURL(
      &worker, completed_url,
      ResolveURICallbacks::Create(success_callback,
                                  ScriptErrorCallback::Wrap(error_callback),
                                  &worker));
}

EntrySync* WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemSyncURL(
    WorkerGlobalScope& worker,
    const String& url,
    ExceptionState& exception_state) {
  KURL completed_url = worker.CompleteURL(url);
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem() ||
      !secure_context->GetSecurityOrigin()->CanRequest(completed_url)) {
    exception_state.ThrowSecurityError(FileError::kSecurityErrorMessage);
    return nullptr;
  }

  if (!completed_url.IsValid()) {
    exception_state.ThrowDOMException(kEncodingError,
                                      "the URL '" + url + "' is invalid.");
    return nullptr;
  }

  EntrySyncCallbackHelper* resolve_url_helper =
      EntrySyncCallbackHelper::Create();
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks =
      ResolveURICallbacks::Create(resolve_url_helper->GetSuccessCallback(),
                                  resolve_url_helper->GetErrorCallback(),
                                  &worker);
  callbacks->SetShouldBlockUntilCompletion(true);

  LocalFileSystem::From(worker)->ResolveURL(&worker, completed_url,
                                            std::move(callbacks));

  return resolve_url_helper->GetResult(exception_state);
}

static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::kTemporary) ==
                  static_cast<int>(kFileSystemTypeTemporary),
              "WorkerGlobalScopeFileSystem::kTemporary should match "
              "FileSystemTypeTemporary");
static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::kPersistent) ==
                  static_cast<int>(kFileSystemTypePersistent),
              "WorkerGlobalScopeFileSystem::kPersistent should match "
              "FileSystemTypePersistent");

}  // namespace blink
