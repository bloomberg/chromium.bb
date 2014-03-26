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
#include "modules/filesystem/FileSystemCallbacks.h"

#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/DirectoryReader.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileWriterBase.h"
#include "modules/filesystem/FileWriterBaseCallback.h"
#include "modules/filesystem/Metadata.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/FileMetadata.h"
#include "public/platform/WebFileWriter.h"

namespace WebCore {

FileSystemCallbacksBase::FileSystemCallbacksBase(PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem)
    : m_errorCallback(errorCallback)
    , m_fileSystem(fileSystem)
{
    if (m_fileSystem)
        m_fileSystem->addPendingCallbacks();
}

FileSystemCallbacksBase::~FileSystemCallbacksBase()
{
    if (m_fileSystem)
        m_fileSystem->removePendingCallbacks();
}

void FileSystemCallbacksBase::didFail(int code)
{
    if (m_errorCallback) {
        m_errorCallback->handleEvent(FileError::create(static_cast<FileError::ErrorCode>(code)).get());
        m_errorCallback.clear();
    }
}

// EntryCallbacks -------------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> EntryCallbacks::create(PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& expectedPath, bool isDirectory)
{
    return adoptPtr(new EntryCallbacks(successCallback, errorCallback, fileSystem, expectedPath, isDirectory));
}

EntryCallbacks::EntryCallbacks(PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& expectedPath, bool isDirectory)
    : FileSystemCallbacksBase(errorCallback, fileSystem)
    , m_successCallback(successCallback)
    , m_expectedPath(expectedPath)
    , m_isDirectory(isDirectory)
{
}

void EntryCallbacks::didSucceed()
{
    if (m_successCallback) {
        if (m_isDirectory)
            m_successCallback->handleEvent(DirectoryEntry::create(m_fileSystem, m_expectedPath).get());
        else
            m_successCallback->handleEvent(FileEntry::create(m_fileSystem, m_expectedPath).get());
    }
    m_successCallback.clear();
}

// EntriesCallbacks -----------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> EntriesCallbacks::create(PassOwnPtr<EntriesCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DirectoryReaderBase> directoryReader, const String& basePath)
{
    return adoptPtr(new EntriesCallbacks(successCallback, errorCallback, directoryReader, basePath));
}

EntriesCallbacks::EntriesCallbacks(PassOwnPtr<EntriesCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DirectoryReaderBase> directoryReader, const String& basePath)
    : FileSystemCallbacksBase(errorCallback, directoryReader->filesystem())
    , m_successCallback(successCallback)
    , m_directoryReader(directoryReader)
    , m_basePath(basePath)
{
    ASSERT(m_directoryReader);
}

void EntriesCallbacks::didReadDirectoryEntry(const String& name, bool isDirectory)
{
    if (isDirectory)
        m_entries.append(DirectoryEntry::create(m_directoryReader->filesystem(), DOMFilePath::append(m_basePath, name)));
    else
        m_entries.append(FileEntry::create(m_directoryReader->filesystem(), DOMFilePath::append(m_basePath, name)));
}

void EntriesCallbacks::didReadDirectoryEntries(bool hasMore)
{
    m_directoryReader->setHasMoreEntries(hasMore);
    EntryHeapVector entries;
    entries.swap(m_entries);
    if (m_successCallback)
        m_successCallback->handleEvent(entries);
}

// FileSystemCallbacks --------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> FileSystemCallbacks::create(PassOwnPtr<FileSystemCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* executionContext, FileSystemType type)
{
    return adoptPtr(new FileSystemCallbacks(successCallback, errorCallback, executionContext, type));
}

FileSystemCallbacks::FileSystemCallbacks(PassOwnPtr<FileSystemCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* context, FileSystemType type)
    : FileSystemCallbacksBase(errorCallback, nullptr)
    , m_successCallback(successCallback)
    , m_executionContext(context)
    , m_type(type)
{
}

void FileSystemCallbacks::didOpenFileSystem(const String& name, const KURL& rootURL)
{
    if (m_successCallback) {
        RefPtrWillBeRawPtr<DOMFileSystem> fileSystem = DOMFileSystem::create(m_executionContext.get(), name, m_type, rootURL);
        m_successCallback->handleEvent(fileSystem.get());
        m_executionContext.clear();
    }
    m_successCallback.clear();
}

// ResolveURICallbacks --------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> ResolveURICallbacks::create(PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* executionContext)
{
    return adoptPtr(new ResolveURICallbacks(successCallback, errorCallback, executionContext));
}

ResolveURICallbacks::ResolveURICallbacks(PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, ExecutionContext* context)
    : FileSystemCallbacksBase(errorCallback, nullptr)
    , m_successCallback(successCallback)
    , m_executionContext(context)
{
}

void ResolveURICallbacks::didResolveURL(const String& name, const KURL& rootURL, FileSystemType type, const String& filePath, bool isDirectory)
{
    RefPtrWillBeRawPtr<DOMFileSystem> filesystem = DOMFileSystem::create(m_executionContext.get(), name, type, rootURL);
    RefPtrWillBeRawPtr<DirectoryEntry> root = filesystem->root();

    String absolutePath;
    if (!DOMFileSystemBase::pathToAbsolutePath(type, root.get(), filePath, absolutePath)) {
        m_errorCallback->handleEvent(FileError::create(FileError::INVALID_MODIFICATION_ERR).get());
        m_errorCallback.clear();
        return;
    }

    if (isDirectory)
        m_successCallback->handleEvent(DirectoryEntry::create(filesystem, absolutePath).get());
    else
        m_successCallback->handleEvent(FileEntry::create(filesystem, absolutePath).get());
    m_successCallback.clear();
}

// MetadataCallbacks ----------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> MetadataCallbacks::create(PassOwnPtr<MetadataCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem)
{
    return adoptPtr(new MetadataCallbacks(successCallback, errorCallback, fileSystem));
}

MetadataCallbacks::MetadataCallbacks(PassOwnPtr<MetadataCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem)
    : FileSystemCallbacksBase(errorCallback, fileSystem)
    , m_successCallback(successCallback)
{
}

void MetadataCallbacks::didReadMetadata(const FileMetadata& metadata)
{
    if (m_successCallback)
        m_successCallback->handleEvent(Metadata::create(metadata).get());
    m_successCallback.clear();
}

// FileWriterBaseCallbacks ----------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> FileWriterBaseCallbacks::create(PassRefPtrWillBeRawPtr<FileWriterBase> fileWriter, PassOwnPtr<FileWriterBaseCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback)
{
    return adoptPtr(new FileWriterBaseCallbacks(fileWriter, successCallback, errorCallback));
}

FileWriterBaseCallbacks::FileWriterBaseCallbacks(PassRefPtrWillBeRawPtr<FileWriterBase> fileWriter, PassOwnPtr<FileWriterBaseCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback)
    : FileSystemCallbacksBase(errorCallback, nullptr)
    , m_fileWriter(fileWriter)
    , m_successCallback(successCallback)
{
}

void FileWriterBaseCallbacks::didCreateFileWriter(PassOwnPtr<blink::WebFileWriter> fileWriter, long long length)
{
    m_fileWriter->initialize(fileWriter, length);
    if (m_successCallback) {
        m_successCallback->handleEvent(m_fileWriter.get());
        m_fileWriter = nullptr;
    }
    m_successCallback.clear();
}

// VoidCallbacks --------------------------------------------------------------

PassOwnPtr<AsyncFileSystemCallbacks> VoidCallbacks::create(PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem)
{
    return adoptPtr(new VoidCallbacks(successCallback, errorCallback, fileSystem));
}

VoidCallbacks::VoidCallbacks(PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem)
    : FileSystemCallbacksBase(errorCallback, fileSystem)
    , m_successCallback(successCallback)
{
}

void VoidCallbacks::didSucceed()
{
    if (m_successCallback)
        m_successCallback->handleEvent();
    m_successCallback.clear();
}

} // namespace
