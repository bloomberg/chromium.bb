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

#ifndef FileSystemCallbacks_h
#define FileSystemCallbacks_h

#include "modules/filesystem/EntriesCallback.h"
#include "platform/AsyncFileSystemCallbacks.h"
#include "platform/FileSystemType.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMFileSystemBase;
class DirectoryReaderBase;
class EntriesCallback;
class EntryCallback;
class ErrorCallback;
class FileCallback;
struct FileMetadata;
class FileSystemCallback;
class FileWriterBase;
class FileWriterBaseCallback;
class MetadataCallback;
class ExecutionContext;
class VoidCallback;

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
public:
    virtual ~FileSystemCallbacksBase();

    // For ErrorCallback.
    virtual void didFail(int code) OVERRIDE FINAL;

    // Other callback methods are implemented by each subclass.

protected:
    FileSystemCallbacksBase(PassOwnPtrWillBeRawPtr<ErrorCallback>, DOMFileSystemBase*, ExecutionContext*);

    bool shouldScheduleCallback() const;

#if !ENABLE(OILPAN)
    template <typename CB, typename CBArg>
    void handleEventOrScheduleCallback(PassOwnPtr<CB>, RawPtr<CBArg>);
#endif

    template <typename CB, typename CBArg>
    void handleEventOrScheduleCallback(PassOwnPtrWillBeRawPtr<CB>, CBArg*);

    template <typename CB, typename CBArg>
    void handleEventOrScheduleCallback(PassOwnPtrWillBeRawPtr<CB>, PassRefPtrWillBeRawPtr<CBArg>);

    template <typename CB>
    void handleEventOrScheduleCallback(PassOwnPtrWillBeRawPtr<CB>);

    OwnPtrWillBePersistent<ErrorCallback> m_errorCallback;
    Persistent<DOMFileSystemBase> m_fileSystem;
    RefPtrWillBePersistent<ExecutionContext> m_executionContext;
    int m_asyncOperationId;
};

// Subclasses ----------------------------------------------------------------

class EntryCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<EntryCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*, const String& expectedPath, bool isDirectory);
    virtual void didSucceed() OVERRIDE;

private:
    EntryCallbacks(PassOwnPtrWillBeRawPtr<EntryCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*, const String& expectedPath, bool isDirectory);
    OwnPtrWillBePersistent<EntryCallback> m_successCallback;
    String m_expectedPath;
    bool m_isDirectory;
};

class EntriesCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<EntriesCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DirectoryReaderBase*, const String& basePath);
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory) OVERRIDE;
    virtual void didReadDirectoryEntries(bool hasMore) OVERRIDE;

private:
    EntriesCallbacks(PassOwnPtrWillBeRawPtr<EntriesCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DirectoryReaderBase*, const String& basePath);
    OwnPtrWillBePersistent<EntriesCallback> m_successCallback;
    Persistent<DirectoryReaderBase> m_directoryReader;
    String m_basePath;
    PersistentHeapVector<Member<Entry> > m_entries;
};

class FileSystemCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<FileSystemCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, FileSystemType);
    virtual void didOpenFileSystem(const String& name, const KURL& rootURL) OVERRIDE;

private:
    FileSystemCallbacks(PassOwnPtrWillBeRawPtr<FileSystemCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, FileSystemType);
    OwnPtrWillBePersistent<FileSystemCallback> m_successCallback;
    FileSystemType m_type;
};

class ResolveURICallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<EntryCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    virtual void didResolveURL(const String& name, const KURL& rootURL, FileSystemType, const String& filePath, bool isDirectry) OVERRIDE;

private:
    ResolveURICallbacks(PassOwnPtrWillBeRawPtr<EntryCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    OwnPtrWillBePersistent<EntryCallback> m_successCallback;
};

class MetadataCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<MetadataCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*);
    virtual void didReadMetadata(const FileMetadata&) OVERRIDE;

private:
    MetadataCallbacks(PassOwnPtrWillBeRawPtr<MetadataCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*);
    OwnPtrWillBePersistent<MetadataCallback> m_successCallback;
};

class FileWriterBaseCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassRefPtrWillBeRawPtr<FileWriterBase>, PassOwnPtrWillBeRawPtr<FileWriterBaseCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    virtual void didCreateFileWriter(PassOwnPtr<WebFileWriter>, long long length) OVERRIDE;

private:
    FileWriterBaseCallbacks(PassRefPtrWillBeRawPtr<FileWriterBase>, PassOwnPtrWillBeRawPtr<FileWriterBaseCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    Persistent<FileWriterBase> m_fileWriter;
    OwnPtrWillBePersistent<FileWriterBaseCallback> m_successCallback;
};

class SnapshotFileCallback FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(DOMFileSystemBase*, const String& name, const KURL&, PassOwnPtrWillBeRawPtr<FileCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    virtual void didCreateSnapshotFile(const FileMetadata&, PassRefPtr<BlobDataHandle> snapshot);

private:
    SnapshotFileCallback(DOMFileSystemBase*, const String& name, const KURL&, PassOwnPtrWillBeRawPtr<FileCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*);
    String m_name;
    KURL m_url;
    OwnPtrWillBePersistent<FileCallback> m_successCallback;
};

class VoidCallbacks FINAL : public FileSystemCallbacksBase {
public:
    static PassOwnPtr<AsyncFileSystemCallbacks> create(PassOwnPtrWillBeRawPtr<VoidCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*);
    virtual void didSucceed() OVERRIDE;

private:
    VoidCallbacks(PassOwnPtrWillBeRawPtr<VoidCallback>, PassOwnPtrWillBeRawPtr<ErrorCallback>, ExecutionContext*, DOMFileSystemBase*);
    OwnPtrWillBePersistent<VoidCallback> m_successCallback;
};

} // namespace blink

#endif // FileSystemCallbacks_h
