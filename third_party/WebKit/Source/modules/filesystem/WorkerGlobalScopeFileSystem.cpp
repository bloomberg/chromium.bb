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

#include <memory>

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/FileError.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/LocalFileSystem.h"
#include "modules/filesystem/SyncCallbackHelper.h"
#include "platform/FileSystemType.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

void WorkerGlobalScopeFileSystem::webkitRequestFileSystem(
    WorkerGlobalScope& worker,
    int type,
    long long size,
    V8FileSystemCallback* success_callback,
    V8ErrorCallback* error_callback) {
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem()) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kSecurityErr);
    return;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
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
      FileSystemCallbacks::Create(
          FileSystemCallbacks::OnDidOpenFileSystemV8Impl::Create(
              success_callback),
          ScriptErrorCallback::Wrap(error_callback), &worker,
          file_system_type));
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
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  FileSystemType file_system_type = static_cast<FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    exception_state.ThrowDOMException(
        kInvalidModificationError,
        "the type must be kTemporary or kPersistent.");
    return nullptr;
  }

  FileSystemCallbacksSyncHelper* sync_helper =
      FileSystemCallbacksSyncHelper::Create();
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks =
      FileSystemCallbacks::Create(sync_helper->GetSuccessCallback(),
                                  sync_helper->GetErrorCallback(), &worker,
                                  file_system_type);
  callbacks->SetShouldBlockUntilCompletion(true);

  LocalFileSystem::From(worker)->RequestFileSystem(&worker, file_system_type,
                                                   size, std::move(callbacks));
  DOMFileSystem* file_system = sync_helper->GetResultOrThrow(exception_state);
  return file_system ? DOMFileSystemSync::Create(file_system) : nullptr;
}

void WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemURL(
    WorkerGlobalScope& worker,
    const String& url,
    V8EntryCallback* success_callback,
    V8ErrorCallback* error_callback) {
  KURL completed_url = worker.CompleteURL(url);
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem() ||
      !secure_context->GetSecurityOrigin()->CanRequest(completed_url)) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kSecurityErr);
    return;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    DOMFileSystem::ReportError(&worker,
                               ScriptErrorCallback::Wrap(error_callback),
                               FileError::kEncodingErr);
    return;
  }

  LocalFileSystem::From(worker)->ResolveURL(
      &worker, completed_url,
      ResolveURICallbacks::Create(
          ResolveURICallbacks::OnDidGetEntryV8Impl::Create(success_callback),
          ScriptErrorCallback::Wrap(error_callback), &worker));
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
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    exception_state.ThrowDOMException(kEncodingError,
                                      "the URL '" + url + "' is invalid.");
    return nullptr;
  }

  EntryCallbacksSyncHelper* sync_helper = EntryCallbacksSyncHelper::Create();
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks =
      ResolveURICallbacks::Create(sync_helper->GetSuccessCallback(),
                                  sync_helper->GetErrorCallback(), &worker);
  callbacks->SetShouldBlockUntilCompletion(true);

  LocalFileSystem::From(worker)->ResolveURL(&worker, completed_url,
                                            std::move(callbacks));

  Entry* entry = sync_helper->GetResultOrThrow(exception_state);
  return entry ? EntrySync::Create(entry) : nullptr;
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
