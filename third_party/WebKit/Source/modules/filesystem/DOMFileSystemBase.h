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

#ifndef DOMFileSystemBase_h
#define DOMFileSystemBase_h

#include "modules/filesystem/FileSystemFlags.h"
#include "modules/filesystem/FileSystemType.h"
#include "weborigin/KURL.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebKit {
class WebFileSystem;
}

namespace WebCore {

class DirectoryEntry;
class DirectoryReaderBase;
class EntriesCallback;
class EntryBase;
class EntryCallback;
class ErrorCallback;
class MetadataCallback;
class ScriptExecutionContext;
class SecurityOrigin;
class VoidCallback;

// A common base class for DOMFileSystem and DOMFileSystemSync.
class DOMFileSystemBase : public RefCounted<DOMFileSystemBase> {
public:
    enum SynchronousType {
        Synchronous,
        Asynchronous,
    };

    // Path prefixes that are used in the filesystem URLs (that can be obtained by toURL()).
    // http://www.w3.org/TR/file-system-api/#widl-Entry-toURL
    static const char persistentPathPrefix[];
    static const char temporaryPathPrefix[];
    static const char isolatedPathPrefix[];
    static const char externalPathPrefix[];

    static PassRefPtr<DOMFileSystemBase> create(ScriptExecutionContext* context, const String& name, FileSystemType type, const KURL& rootURL)
    {
        return adoptRef(new DOMFileSystemBase(context, name, type, rootURL));
    }
    virtual ~DOMFileSystemBase();

    // These are called when a new callback is created and resolved in
    // FileSystem API, so that subclasses can track the number of pending
    // callbacks if necessary.
    virtual void addPendingCallbacks() { }
    virtual void removePendingCallbacks() { }

    const String& name() const { return m_name; }
    FileSystemType type() const { return m_type; }
    KURL rootURL() const { return m_filesystemRootURL; }
    WebKit::WebFileSystem* fileSystem() const;
    SecurityOrigin* securityOrigin() const;

    // The clonable flag is used in the structured clone algorithm to test
    // whether the FileSystem API object is permitted to be cloned. It defaults
    // to false, and must be explicitly set by internal code permit cloning.
    void makeClonable() { m_clonable = true; }
    bool clonable() const { return m_clonable; }

    static bool isValidType(FileSystemType);
    static bool crackFileSystemURL(const KURL&, FileSystemType&, String& filePath);
    bool supportsToURL() const;
    KURL createFileSystemURL(const EntryBase*) const;
    KURL createFileSystemURL(const String& fullPath) const;

    // Actual FileSystem API implementations. All the validity checks on virtual paths are done at this level.
    // They return false for immediate errors that don't involve lower WebFileSystem layer (e.g. for name validation errors). Otherwise they return true (but later may call back with an runtime error).
    bool getMetadata(const EntryBase*, PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool move(const EntryBase* source, EntryBase* parent, const String& name, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool copy(const EntryBase* source, EntryBase* parent, const String& name, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool remove(const EntryBase*, PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool removeRecursively(const EntryBase*, PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool getParent(const EntryBase*, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>);
    bool getFile(const EntryBase*, const String& path, const FileSystemFlags&, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool getDirectory(const EntryBase*, const String& path, const FileSystemFlags&, PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);
    bool readDirectory(PassRefPtr<DirectoryReaderBase>, const String& path, PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, SynchronousType = Asynchronous);

protected:
    DOMFileSystemBase(ScriptExecutionContext*, const String& name, FileSystemType, const KURL& rootURL);
    friend class DOMFileSystemSync;

    ScriptExecutionContext* m_context;
    String m_name;
    FileSystemType m_type;
    KURL m_filesystemRootURL;
    bool m_clonable;
};

inline bool operator==(const DOMFileSystemBase& a, const DOMFileSystemBase& b) { return a.name() == b.name() && a.type() == b.type() && a.rootURL() == b.rootURL(); }

} // namespace WebCore

#endif // DOMFileSystemBase_h
