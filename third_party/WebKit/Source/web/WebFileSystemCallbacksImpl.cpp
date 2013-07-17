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
#include "config.h"
#include "WebFileSystemCallbacksImpl.h"

#include "AsyncFileSystemChromium.h"
#include "AsyncFileWriterChromium.h"
#include "WorkerAsyncFileSystemChromium.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/platform/AsyncFileSystemCallbacks.h"
#include "core/platform/FileMetadata.h"
#include "public/platform/WebFileInfo.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemEntry.h"
#include "public/platform/WebString.h"
#include "public/web/WebFileWriter.h"
#include "wtf/Vector.h"

using namespace WebCore;

namespace WebKit {

WebFileSystemCallbacksImpl::WebFileSystemCallbacksImpl(PassOwnPtr<AsyncFileSystemCallbacks> callbacks, ScriptExecutionContext* context, FileSystemSynchronousType synchronousType)
    : m_callbacks(callbacks)
    , m_context(context)
    , m_synchronousType(synchronousType)
{
    ASSERT(m_callbacks);
}

WebFileSystemCallbacksImpl::WebFileSystemCallbacksImpl(PassOwnPtr<AsyncFileSystemCallbacks> callbacks, PassOwnPtr<AsyncFileWriterChromium> writer)
    : m_callbacks(callbacks)
    , m_context(0)
    , m_writer(writer)
{
    ASSERT(m_callbacks);
}

WebFileSystemCallbacksImpl::~WebFileSystemCallbacksImpl()
{
}

void WebFileSystemCallbacksImpl::didSucceed()
{
    m_callbacks->didSucceed();
    delete this;
}

void WebFileSystemCallbacksImpl::didReadMetadata(const WebFileInfo& webFileInfo)
{
    FileMetadata fileMetadata;
    fileMetadata.modificationTime = webFileInfo.modificationTime;
    fileMetadata.length = webFileInfo.length;
    fileMetadata.type = static_cast<FileMetadata::Type>(webFileInfo.type);
    fileMetadata.platformPath = webFileInfo.platformPath;
    m_callbacks->didReadMetadata(fileMetadata);
    delete this;
}

void WebFileSystemCallbacksImpl::didCreateSnapshotFile(const WebFileInfo& webFileInfo)
{
    // It's important to create a BlobDataHandle that refers to the platform file path prior
    // to return from this method so the underlying file will not be deleted.
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->appendFile(webFileInfo.platformPath);
    RefPtr<BlobDataHandle> snapshotBlob = BlobDataHandle::create(blobData.release(), webFileInfo.length);
    didCreateSnapshotFile(webFileInfo, snapshotBlob);
}

void WebFileSystemCallbacksImpl::didCreateSnapshotFile(const WebFileInfo& webFileInfo, PassRefPtr<WebCore::BlobDataHandle> snapshot)
{
    FileMetadata fileMetadata;
    fileMetadata.modificationTime = webFileInfo.modificationTime;
    fileMetadata.length = webFileInfo.length;
    fileMetadata.type = static_cast<FileMetadata::Type>(webFileInfo.type);
    fileMetadata.platformPath = webFileInfo.platformPath;
    m_callbacks->didCreateSnapshotFile(fileMetadata, snapshot);
    delete this;
}

void WebFileSystemCallbacksImpl::didReadDirectory(const WebVector<WebFileSystemEntry>& entries, bool hasMore)
{
    for (size_t i = 0; i < entries.size(); ++i)
        m_callbacks->didReadDirectoryEntry(entries[i].name, entries[i].isDirectory);
    m_callbacks->didReadDirectoryEntries(hasMore);
    delete this;
}

void WebFileSystemCallbacksImpl::didOpenFileSystem(const WebString& name, const WebURL& rootURL)
{
    // This object is intended to delete itself on exit.
    OwnPtr<WebFileSystemCallbacksImpl> callbacks = adoptPtr(this);

    if (m_context && m_context->isWorkerGlobalScope()) {
        m_callbacks->didOpenFileSystem(name, rootURL, WorkerAsyncFileSystemChromium::create(m_context, m_synchronousType));
        return;
    }
    m_callbacks->didOpenFileSystem(name, rootURL, AsyncFileSystemChromium::create());
}

void WebFileSystemCallbacksImpl::didCreateFileWriter(WebFileWriter* webFileWriter, long long length)
{
    // This object is intended to delete itself on exit.
    OwnPtr<WebFileSystemCallbacksImpl> callbacks = adoptPtr(this);

    m_writer->setWebFileWriter(adoptPtr(webFileWriter));
    m_callbacks->didCreateFileWriter(m_writer.release(), length);
}

void WebFileSystemCallbacksImpl::didFail(WebFileError error)
{
    m_callbacks->didFail(error);
    delete this;
}

bool WebFileSystemCallbacksImpl::shouldBlockUntilCompletion() const
{
    return m_callbacks->shouldBlockUntilCompletion();
}

} // namespace WebKit
