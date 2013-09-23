/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/filesystem/LocalFileSystem.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/FileError.h"
#include "core/page/Page.h"
#include "core/platform/AsyncFileSystemCallbacks.h"
#include "modules/filesystem/FileSystemClient.h"
#include "modules/filesystem/WorkerLocalFileSystem.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFileSystem.h"

namespace WebCore {

namespace {

void fileSystemNotAllowed(ScriptExecutionContext*, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    callbacks->didFail(FileError::ABORT_ERR);
}

} // namespace

LocalFileSystemBase::~LocalFileSystemBase()
{
}

void LocalFileSystemBase::readFileSystem(ScriptExecutionContext* context, FileSystemType type, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    if (!client() || !client()->allowFileSystem(context)) {
        context->postTask(createCallbackTask(&fileSystemNotAllowed, callbacks));
        return;
    }
    KURL storagePartition = KURL(KURL(), context->securityOrigin()->toString());
    WebKit::Platform::current()->fileSystem()->openFileSystem(storagePartition, static_cast<WebKit::WebFileSystemType>(type), false, callbacks);
}

void LocalFileSystemBase::requestFileSystem(ScriptExecutionContext* context, FileSystemType type, long long size, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    if (!client() || !client()->allowFileSystem(context)) {
        context->postTask(createCallbackTask(&fileSystemNotAllowed, callbacks));
        return;
    }
    KURL storagePartition = KURL(KURL(), context->securityOrigin()->toString());
    WebKit::Platform::current()->fileSystem()->openFileSystem(storagePartition, static_cast<WebKit::WebFileSystemType>(type), true, callbacks);
}

void LocalFileSystemBase::deleteFileSystem(ScriptExecutionContext* context, FileSystemType type, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    ASSERT(context);
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());

    if (!client() || !client()->allowFileSystem(context)) {
        context->postTask(createCallbackTask(&fileSystemNotAllowed, callbacks));
        return;
    }
    KURL storagePartition = KURL(KURL(), context->securityOrigin()->toString());
    WebKit::Platform::current()->fileSystem()->deleteFileSystem(storagePartition, static_cast<WebKit::WebFileSystemType>(type), callbacks);
}

LocalFileSystemBase::LocalFileSystemBase(PassOwnPtr<FileSystemClient> client)
    : m_client(client)
{
}

PassOwnPtr<LocalFileSystem> LocalFileSystem::create(PassOwnPtr<FileSystemClient> client)
{
    return adoptPtr(new LocalFileSystem(client));
}

const char* LocalFileSystem::supplementName()
{
    return "LocalFileSystem";
}

LocalFileSystem::LocalFileSystem(PassOwnPtr<FileSystemClient> client)
    : LocalFileSystemBase(client)
{
}

LocalFileSystem::~LocalFileSystem()
{
}

LocalFileSystem* LocalFileSystem::from(ScriptExecutionContext* context)
{
    return static_cast<LocalFileSystem*>(Supplement<Page>::from(toDocument(context)->page(), supplementName()));
}

void provideLocalFileSystemTo(Page* page, PassOwnPtr<FileSystemClient> client)
{
    LocalFileSystem::provideTo(page, LocalFileSystem::supplementName(), LocalFileSystem::create(client));
}

} // namespace WebCore
