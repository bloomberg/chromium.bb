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

#include <memory>

#include "core/fileapi/FileError.h"
#include "platform/AsyncFileSystemCallbacks.h"
#include "platform/FileSystemType.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMFileSystemBase;
class DirectoryReaderBase;
class DirectoryReaderOnDidReadCallback;
class Entry;
class ExecutionContext;
class FileCallback;
class FileMetadata;
class FileSystemCallback;
class FileWriterBase;
class FileWriterBaseCallback;
class MetadataCallback;
class V8EntryCallback;
class V8ErrorCallback;
class VoidCallback;

// Passed to DOMFileSystem implementations that may report errors. Subclasses
// may capture the error for throwing on return to script (for synchronous APIs)
// or call an actual script callback (for asynchronous APIs).
class ErrorCallbackBase : public GarbageCollectedFinalized<ErrorCallbackBase> {
 public:
  virtual ~ErrorCallbackBase() {}
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void Invoke(FileError::ErrorCode) = 0;
};

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
 public:
  ~FileSystemCallbacksBase() override;

  // For ErrorCallback.
  void DidFail(int code) final;

  // Other callback methods are implemented by each subclass.

 protected:
  FileSystemCallbacksBase(ErrorCallbackBase*,
                          DOMFileSystemBase*,
                          ExecutionContext*);

  bool ShouldScheduleCallback() const;

  template <typename CB, typename CBArg>
  void HandleEventOrScheduleCallback(CB*, CBArg*);

  template <typename CB>
  void HandleEventOrScheduleCallback(CB*);

  // Invokes the given callback synchronously or asynchronously depending on
  // the result of |ShouldScheduleCallback|.
  template <typename CallbackMemberFunction,
            typename CallbackClass,
            typename... Args>
  void InvokeOrScheduleCallback(CallbackMemberFunction&&,
                                CallbackClass&&,
                                Args&&...);

  Persistent<ErrorCallbackBase> error_callback_;
  Persistent<DOMFileSystemBase> file_system_;
  Persistent<ExecutionContext> execution_context_;
  int async_operation_id_;
};

// Subclasses ----------------------------------------------------------------

// Wraps a script-provided callback for use in DOMFileSystem operations.
class ScriptErrorCallback final : public ErrorCallbackBase {
 public:
  static ScriptErrorCallback* Wrap(V8ErrorCallback*);
  ~ScriptErrorCallback() override {}
  void Trace(blink::Visitor*) override;

  void Invoke(FileError::ErrorCode) override;

 private:
  explicit ScriptErrorCallback(V8ErrorCallback*);
  Member<V8ErrorCallback> callback_;
};

class EntryCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidGetEntryCallback
      : public GarbageCollectedFinalized<OnDidGetEntryCallback> {
   public:
    virtual ~OnDidGetEntryCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(Entry*) = 0;

   protected:
    OnDidGetEntryCallback() = default;
  };

  class OnDidGetEntryV8Impl : public OnDidGetEntryCallback {
   public:
    static OnDidGetEntryV8Impl* Create(V8EntryCallback* callback) {
      return callback ? new OnDidGetEntryV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(Entry*) override;

   private:
    OnDidGetEntryV8Impl(V8EntryCallback* callback) : callback_(callback) {}

    Member<V8EntryCallback> callback_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      OnDidGetEntryCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DOMFileSystemBase*,
      const String& expected_path,
      bool is_directory);
  void DidSucceed() override;

 private:
  EntryCallbacks(OnDidGetEntryCallback*,
                 ErrorCallbackBase*,
                 ExecutionContext*,
                 DOMFileSystemBase*,
                 const String& expected_path,
                 bool is_directory);
  Persistent<OnDidGetEntryCallback> success_callback_;
  String expected_path_;
  bool is_directory_;
};

class EntriesCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      DirectoryReaderOnDidReadCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DirectoryReaderBase*,
      const String& base_path);
  void DidReadDirectoryEntry(const String& name, bool is_directory) override;
  void DidReadDirectoryEntries(bool has_more) override;

 private:
  EntriesCallbacks(DirectoryReaderOnDidReadCallback*,
                   ErrorCallbackBase*,
                   ExecutionContext*,
                   DirectoryReaderBase*,
                   const String& base_path);
  Persistent<DirectoryReaderOnDidReadCallback> success_callback_;
  Persistent<DirectoryReaderBase> directory_reader_;
  String base_path_;
  PersistentHeapVector<Member<Entry>> entries_;
};

class FileSystemCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(FileSystemCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          FileSystemType);
  void DidOpenFileSystem(const String& name, const KURL& root_url) override;

 private:
  FileSystemCallbacks(FileSystemCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*,
                      FileSystemType);
  Persistent<FileSystemCallback> success_callback_;
  FileSystemType type_;
};

class ResolveURICallbacks final : public FileSystemCallbacksBase {
 public:
  using OnDidGetEntryCallback = EntryCallbacks::OnDidGetEntryCallback;
  using OnDidGetEntryV8Impl = EntryCallbacks::OnDidGetEntryV8Impl;

  static std::unique_ptr<AsyncFileSystemCallbacks>
  Create(OnDidGetEntryCallback*, ErrorCallbackBase*, ExecutionContext*);
  void DidResolveURL(const String& name,
                     const KURL& root_url,
                     FileSystemType,
                     const String& file_path,
                     bool is_directry) override;

 private:
  ResolveURICallbacks(OnDidGetEntryCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*);
  Persistent<OnDidGetEntryCallback> success_callback_;
};

class MetadataCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(MetadataCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          DOMFileSystemBase*);
  void DidReadMetadata(const FileMetadata&) override;

 private:
  MetadataCallbacks(MetadataCallback*,
                    ErrorCallbackBase*,
                    ExecutionContext*,
                    DOMFileSystemBase*);
  Persistent<MetadataCallback> success_callback_;
};

class FileWriterBaseCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      FileWriterBase*,
      FileWriterBaseCallback*,
      ErrorCallbackBase*,
      ExecutionContext*);
  void DidCreateFileWriter(std::unique_ptr<WebFileWriter>,
                           long long length) override;

 private:
  FileWriterBaseCallbacks(FileWriterBase*,
                          FileWriterBaseCallback*,
                          ErrorCallbackBase*,
                          ExecutionContext*);
  Persistent<FileWriterBase> file_writer_;
  Persistent<FileWriterBaseCallback> success_callback_;
};

class SnapshotFileCallback final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(DOMFileSystemBase*,
                                                          const String& name,
                                                          const KURL&,
                                                          FileCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*);
  void DidCreateSnapshotFile(const FileMetadata&,
                             scoped_refptr<BlobDataHandle> snapshot) override;

 private:
  SnapshotFileCallback(DOMFileSystemBase*,
                       const String& name,
                       const KURL&,
                       FileCallback*,
                       ErrorCallbackBase*,
                       ExecutionContext*);
  String name_;
  KURL url_;
  Persistent<FileCallback> success_callback_;
};

class VoidCallbacks final : public FileSystemCallbacksBase {
 public:
  static std::unique_ptr<AsyncFileSystemCallbacks> Create(VoidCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          DOMFileSystemBase*);
  void DidSucceed() override;

 private:
  VoidCallbacks(VoidCallback*,
                ErrorCallbackBase*,
                ExecutionContext*,
                DOMFileSystemBase*);
  Persistent<VoidCallback> success_callback_;
};

}  // namespace blink

#endif  // FileSystemCallbacks_h
