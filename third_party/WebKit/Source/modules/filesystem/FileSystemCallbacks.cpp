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

#include "modules/filesystem/FileSystemCallbacks.h"

#include <memory>
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/BlobCallback.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DOMFileSystem.h"
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
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebFileWriter.h"

namespace blink {

FileSystemCallbacksBase::FileSystemCallbacksBase(
    ErrorCallbackBase* error_callback,
    DOMFileSystemBase* file_system,
    ExecutionContext* context)
    : error_callback_(error_callback),
      file_system_(file_system),
      execution_context_(context) {
  if (file_system_)
    file_system_->AddPendingCallbacks();
}

FileSystemCallbacksBase::~FileSystemCallbacksBase() {
  if (file_system_)
    file_system_->RemovePendingCallbacks();
}

void FileSystemCallbacksBase::DidFail(int code) {
  if (error_callback_)
    InvokeOrScheduleCallback(error_callback_.Release(),
                             static_cast<FileError::ErrorCode>(code));
}

bool FileSystemCallbacksBase::ShouldScheduleCallback() const {
  return !ShouldBlockUntilCompletion() && execution_context_ &&
         execution_context_->IsContextSuspended();
}

template <typename CB, typename CBArg>
void FileSystemCallbacksBase::InvokeOrScheduleCallback(CB* callback,
                                                       CBArg arg) {
  DCHECK(callback);
  if (callback) {
    if (ShouldScheduleCallback()) {
      DOMFileSystem::ScheduleCallback(
          execution_context_.Get(),
          WTF::Bind(&CB::Invoke, WrapPersistent(callback), arg));
    } else {
      callback->Invoke(arg);
    }
  }
  execution_context_.Clear();
}

template <typename CB, typename CBArg>
void FileSystemCallbacksBase::HandleEventOrScheduleCallback(CB* callback,
                                                            CBArg* arg) {
  DCHECK(callback);
  if (callback) {
    if (ShouldScheduleCallback()) {
      DOMFileSystem::ScheduleCallback(
          execution_context_.Get(),
          WTF::Bind(&CB::handleEvent, WrapPersistent(callback),
                    WrapPersistent(arg)));
    } else {
      callback->handleEvent(arg);
    }
  }
  execution_context_.Clear();
}

template <typename CB>
void FileSystemCallbacksBase::HandleEventOrScheduleCallback(CB* callback) {
  DCHECK(callback);
  if (callback) {
    if (ShouldScheduleCallback()) {
      DOMFileSystem::ScheduleCallback(
          execution_context_.Get(),
          WTF::Bind(&CB::handleEvent, WrapPersistent(callback)));
    } else {
      callback->handleEvent();
    }
  }
  execution_context_.Clear();
}

// ScriptErrorCallback --------------------------------------------------------

// static
ScriptErrorCallback* ScriptErrorCallback::Wrap(ErrorCallback* callback) {
  // DOMFileSystem operations take an optional (nullable) callback. If a
  // script callback was not passed, don't bother creating a dummy wrapper
  // and checking during invoke().
  if (!callback)
    return nullptr;
  return new ScriptErrorCallback(callback);
}

void ScriptErrorCallback::Trace(blink::Visitor* visitor) {
  ErrorCallbackBase::Trace(visitor);
  visitor->Trace(callback_);
}

void ScriptErrorCallback::Invoke(FileError::ErrorCode error) {
  callback_->handleEvent(FileError::CreateDOMException(error));
};

ScriptErrorCallback::ScriptErrorCallback(ErrorCallback* callback)
    : callback_(callback) {}

// EntryCallbacks -------------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> EntryCallbacks::Create(
    EntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system,
    const String& expected_path,
    bool is_directory) {
  return WTF::WrapUnique(new EntryCallbacks(success_callback, error_callback,
                                            context, file_system, expected_path,
                                            is_directory));
}

EntryCallbacks::EntryCallbacks(EntryCallback* success_callback,
                               ErrorCallbackBase* error_callback,
                               ExecutionContext* context,
                               DOMFileSystemBase* file_system,
                               const String& expected_path,
                               bool is_directory)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback),
      expected_path_(expected_path),
      is_directory_(is_directory) {}

void EntryCallbacks::DidSucceed() {
  if (success_callback_) {
    if (is_directory_)
      HandleEventOrScheduleCallback(
          success_callback_.Release(),
          DirectoryEntry::Create(file_system_, expected_path_));
    else
      HandleEventOrScheduleCallback(
          success_callback_.Release(),
          FileEntry::Create(file_system_, expected_path_));
  }
}

// EntriesCallbacks -----------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> EntriesCallbacks::Create(
    EntriesCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DirectoryReaderBase* directory_reader,
    const String& base_path) {
  return WTF::WrapUnique(new EntriesCallbacks(
      success_callback, error_callback, context, directory_reader, base_path));
}

EntriesCallbacks::EntriesCallbacks(EntriesCallback* success_callback,
                                   ErrorCallbackBase* error_callback,
                                   ExecutionContext* context,
                                   DirectoryReaderBase* directory_reader,
                                   const String& base_path)
    : FileSystemCallbacksBase(error_callback,
                              directory_reader->Filesystem(),
                              context),
      success_callback_(success_callback),
      directory_reader_(directory_reader),
      base_path_(base_path) {
  DCHECK(directory_reader_);
}

void EntriesCallbacks::DidReadDirectoryEntry(const String& name,
                                             bool is_directory) {
  if (is_directory)
    entries_.push_back(
        DirectoryEntry::Create(directory_reader_->Filesystem(),
                               DOMFilePath::Append(base_path_, name)));
  else
    entries_.push_back(
        FileEntry::Create(directory_reader_->Filesystem(),
                          DOMFilePath::Append(base_path_, name)));
}

void EntriesCallbacks::DidReadDirectoryEntries(bool has_more) {
  directory_reader_->SetHasMoreEntries(has_more);
  EntryHeapVector entries;
  entries.swap(entries_);
  // FIXME: delay the callback iff shouldScheduleCallback() is true.
  if (success_callback_)
    success_callback_->handleEvent(entries);
}

// FileSystemCallbacks --------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> FileSystemCallbacks::Create(
    FileSystemCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    FileSystemType type) {
  return WTF::WrapUnique(
      new FileSystemCallbacks(success_callback, error_callback, context, type));
}

FileSystemCallbacks::FileSystemCallbacks(FileSystemCallback* success_callback,
                                         ErrorCallbackBase* error_callback,
                                         ExecutionContext* context,
                                         FileSystemType type)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback),
      type_(type) {}

void FileSystemCallbacks::DidOpenFileSystem(const String& name,
                                            const KURL& root_url) {
  if (success_callback_)
    HandleEventOrScheduleCallback(
        success_callback_.Release(),
        DOMFileSystem::Create(execution_context_.Get(), name, type_, root_url));
}

// ResolveURICallbacks --------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> ResolveURICallbacks::Create(
    EntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return WTF::WrapUnique(
      new ResolveURICallbacks(success_callback, error_callback, context));
}

ResolveURICallbacks::ResolveURICallbacks(EntryCallback* success_callback,
                                         ErrorCallbackBase* error_callback,
                                         ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback) {}

void ResolveURICallbacks::DidResolveURL(const String& name,
                                        const KURL& root_url,
                                        FileSystemType type,
                                        const String& file_path,
                                        bool is_directory) {
  DOMFileSystem* filesystem =
      DOMFileSystem::Create(execution_context_.Get(), name, type, root_url);
  DirectoryEntry* root = filesystem->root();

  String absolute_path;
  if (!DOMFileSystemBase::PathToAbsolutePath(type, root, file_path,
                                             absolute_path)) {
    InvokeOrScheduleCallback(error_callback_.Release(),
                             FileError::kInvalidModificationErr);
    return;
  }

  if (is_directory)
    HandleEventOrScheduleCallback(
        success_callback_.Release(),
        DirectoryEntry::Create(filesystem, absolute_path));
  else
    HandleEventOrScheduleCallback(success_callback_.Release(),
                                  FileEntry::Create(filesystem, absolute_path));
}

// MetadataCallbacks ----------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> MetadataCallbacks::Create(
    MetadataCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return WTF::WrapUnique(new MetadataCallbacks(success_callback, error_callback,
                                               context, file_system));
}

MetadataCallbacks::MetadataCallbacks(MetadataCallback* success_callback,
                                     ErrorCallbackBase* error_callback,
                                     ExecutionContext* context,
                                     DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void MetadataCallbacks::DidReadMetadata(const FileMetadata& metadata) {
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release(),
                                  Metadata::Create(metadata));
}

// FileWriterBaseCallbacks ----------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> FileWriterBaseCallbacks::Create(
    FileWriterBase* file_writer,
    FileWriterBaseCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return WTF::WrapUnique(new FileWriterBaseCallbacks(
      file_writer, success_callback, error_callback, context));
}

FileWriterBaseCallbacks::FileWriterBaseCallbacks(
    FileWriterBase* file_writer,
    FileWriterBaseCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      file_writer_(file_writer),
      success_callback_(success_callback) {}

void FileWriterBaseCallbacks::DidCreateFileWriter(
    std::unique_ptr<WebFileWriter> file_writer,
    long long length) {
  file_writer_->Initialize(std::move(file_writer), length);
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release(),
                                  file_writer_.Release());
}

// SnapshotFileCallback -------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> SnapshotFileCallback::Create(
    DOMFileSystemBase* filesystem,
    const String& name,
    const KURL& url,
    BlobCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return WTF::WrapUnique(new SnapshotFileCallback(
      filesystem, name, url, success_callback, error_callback, context));
}

SnapshotFileCallback::SnapshotFileCallback(DOMFileSystemBase* filesystem,
                                           const String& name,
                                           const KURL& url,
                                           BlobCallback* success_callback,
                                           ErrorCallbackBase* error_callback,
                                           ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, filesystem, context),
      name_(name),
      url_(url),
      success_callback_(success_callback) {}

void SnapshotFileCallback::DidCreateSnapshotFile(
    const FileMetadata& metadata,
    scoped_refptr<BlobDataHandle> snapshot) {
  if (!success_callback_)
    return;

  // We can't directly use the snapshot blob data handle because the content
  // type on it hasn't been set.  The |snapshot| param is here to provide a a
  // chain of custody thru thread bridging that is held onto until *after* we've
  // coined a File with a new handle that has the correct type set on it. This
  // allows the blob storage system to track when a temp file can and can't be
  // safely deleted.

  HandleEventOrScheduleCallback(
      success_callback_.Release(),
      DOMFileSystemBase::CreateFile(metadata, url_, file_system_->GetType(),
                                    name_));
}

// VoidCallbacks --------------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> VoidCallbacks::Create(
    VoidCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return WTF::WrapUnique(new VoidCallbacks(success_callback, error_callback,
                                           context, file_system));
}

VoidCallbacks::VoidCallbacks(VoidCallback* success_callback,
                             ErrorCallbackBase* error_callback,
                             ExecutionContext* context,
                             DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void VoidCallbacks::DidSucceed() {
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release());
}

}  // namespace blink
