// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_operation_runner.h"

#include "base/bind.h"
#include "net/url_request/url_request_context.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_impl.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace fileapi {

typedef FileSystemOperationRunner::OperationID OperationID;

const OperationID FileSystemOperationRunner::kErrorOperationID = -1;

FileSystemOperationRunner::~FileSystemOperationRunner() {
}

void FileSystemOperationRunner::Shutdown() {
  operations_.Clear();
}

OperationID FileSystemOperationRunner::CreateFile(
    const FileSystemURL& url,
    bool exclusive,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->CreateFile(
      url, exclusive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::CreateDirectory(
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->CreateDirectory(
      url, exclusive, recursive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(dest_url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, dest_url);
  PrepareForRead(id, src_url);
  operation->Copy(
      src_url, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Move(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(dest_url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, dest_url);
  PrepareForWrite(id, src_url);
  operation->Move(
      src_url, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::DirectoryExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForRead(id, url);
  operation->DirectoryExists(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::FileExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForRead(id, url);
  operation->FileExists(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::GetMetadata(
    const FileSystemURL& url,
    const GetMetadataCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error, base::PlatformFileInfo());
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForRead(id, url);
  operation->GetMetadata(
      url,
      base::Bind(&FileSystemOperationRunner::DidGetMetadata, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error, std::vector<DirectoryEntry>(), false);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForRead(id, url);
  operation->ReadDirectory(
      url,
      base::Bind(&FileSystemOperationRunner::DidReadDirectory, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Remove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->Remove(
      url, recursive,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Write(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error, 0, true);
    return kErrorOperationID;
  }

  scoped_ptr<FileStreamWriter> writer(
      file_system_context_->CreateFileStreamWriter(url, offset));
  if (!writer) {
    // Write is not supported.
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY, 0, true);
    return kErrorOperationID;
  }

  DCHECK(blob_url.is_valid());
  scoped_ptr<FileWriterDelegate> writer_delegate(
      new FileWriterDelegate(writer.Pass()));
  scoped_ptr<net::URLRequest> blob_request(url_request_context->CreateRequest(
      blob_url, writer_delegate.get()));

  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->Write(
      url, writer_delegate.Pass(), blob_request.Pass(),
      base::Bind(&FileSystemOperationRunner::DidWrite, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Truncate(
    const FileSystemURL& url, int64 length,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->Truncate(
      url, length,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

void FileSystemOperationRunner::Cancel(
    OperationID id,
    const StatusCallback& callback) {
  FileSystemOperation* operation = operations_.Lookup(id);
  if (!operation) {
    // The operation is already finished; report that we failed to stop it.
    callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }
  operation->Cancel(callback);
}

OperationID FileSystemOperationRunner::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForWrite(id, url);
  operation->TouchFile(
      url, last_access_time, last_modified_time,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::OpenFile(
    const FileSystemURL& url,
    int file_flags,
    base::ProcessHandle peer_handle,
    const OpenFileCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error, base::kInvalidPlatformFileValue,
                 base::Closure(), base::ProcessHandle());
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  if (file_flags &
      (base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_OPEN_ALWAYS |
       base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_TRUNCATED |
       base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE |
       base::PLATFORM_FILE_DELETE_ON_CLOSE |
       base::PLATFORM_FILE_WRITE_ATTRIBUTES)) {
    PrepareForWrite(id, url);
  } else {
    PrepareForRead(id, url);
  }
  operation->OpenFile(
      url, file_flags, peer_handle,
      base::Bind(&FileSystemOperationRunner::DidOpenFile, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::CreateSnapshotFile(
    const FileSystemURL& url,
    const SnapshotFileCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, &error);
  if (!operation) {
    callback.Run(error, base::PlatformFileInfo(), base::FilePath(), NULL);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  PrepareForRead(id, url);
  operation->CreateSnapshotFile(
      url,
      base::Bind(&FileSystemOperationRunner::DidCreateSnapshot, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::CopyInForeignFile(
    const base::FilePath& src_local_disk_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(
      dest_url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  operation->AsFileSystemOperationImpl()->CopyInForeignFile(
      src_local_disk_path, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::RemoveFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  operation->AsFileSystemOperationImpl()->RemoveFile(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::RemoveDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  operation->AsFileSystemOperationImpl()->RemoveDirectory(
      url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(
      src_url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  operation->AsFileSystemOperationImpl()->CopyFileLocal(
      src_url, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

OperationID FileSystemOperationRunner::MoveFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(
      src_url, &error);
  if (!operation) {
    callback.Run(error);
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
  operation->AsFileSystemOperationImpl()->MoveFileLocal(
      src_url, dest_url,
      base::Bind(&FileSystemOperationRunner::DidFinish, AsWeakPtr(),
                 id, callback));
  return id;
}

base::PlatformFileError FileSystemOperationRunner::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = CreateFileSystemOperationImpl(url, &error);
  if (!operation)
    return error;

  return operation->AsFileSystemOperationImpl()->SyncGetPlatformPath(
      url, platform_path);
}

FileSystemOperationRunner::FileSystemOperationRunner(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {}

void FileSystemOperationRunner::DidFinish(
    OperationID id,
    const StatusCallback& callback,
    base::PlatformFileError rv) {
  callback.Run(rv);
  FinishOperation(id);
}

void FileSystemOperationRunner::DidGetMetadata(
    OperationID id,
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  callback.Run(rv, file_info);
  FinishOperation(id);
}

void FileSystemOperationRunner::DidReadDirectory(
    OperationID id,
    const ReadDirectoryCallback& callback,
    base::PlatformFileError rv,
    const std::vector<DirectoryEntry>& entries,
    bool has_more) {
  callback.Run(rv, entries, has_more);
  if (rv != base::PLATFORM_FILE_OK || !has_more)
    FinishOperation(id);
}

void FileSystemOperationRunner::DidWrite(
    OperationID id,
    const WriteCallback& callback,
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  callback.Run(rv, bytes, complete);
  if (rv != base::PLATFORM_FILE_OK || complete)
    FinishOperation(id);
}

void FileSystemOperationRunner::DidOpenFile(
    OperationID id,
    const OpenFileCallback& callback,
    base::PlatformFileError rv,
    base::PlatformFile file,
    const base::Closure& on_close_callback,
    base::ProcessHandle peer_handle) {
  callback.Run(rv, file, on_close_callback, peer_handle);
  FinishOperation(id);
}

void FileSystemOperationRunner::DidCreateSnapshot(
    OperationID id,
    const SnapshotFileCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  callback.Run(rv, file_info, platform_path, file_ref);
  FinishOperation(id);
}

FileSystemOperation*
FileSystemOperationRunner::CreateFileSystemOperationImpl(
    const FileSystemURL& url, base::PlatformFileError* error) {
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url, error);
  if (!operation)
    return NULL;
  if (!operation->AsFileSystemOperationImpl()) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
    delete operation;
    return NULL;
  }
  return operation;
}

void FileSystemOperationRunner::PrepareForWrite(OperationID id,
                                                const FileSystemURL& url) {
  if (file_system_context_->GetUpdateObservers(url.type())) {
    file_system_context_->GetUpdateObservers(url.type())->Notify(
        &FileUpdateObserver::OnStartUpdate, MakeTuple(url));
  }
  write_target_urls_[id].insert(url);
}

void FileSystemOperationRunner::PrepareForRead(OperationID id,
                                               const FileSystemURL& url) {
  if (file_system_context_->GetAccessObservers(url.type())) {
    file_system_context_->GetAccessObservers(url.type())->Notify(
        &FileAccessObserver::OnAccess, MakeTuple(url));
  }
}

void FileSystemOperationRunner::FinishOperation(OperationID id) {
  OperationToURLSet::iterator found = write_target_urls_.find(id);
  if (found != write_target_urls_.end()) {
    const FileSystemURLSet& urls = found->second;
    for (FileSystemURLSet::const_iterator iter = urls.begin();
        iter != urls.end(); ++iter) {
      if (file_system_context_->GetUpdateObservers(iter->type())) {
        file_system_context_->GetUpdateObservers(iter->type())->Notify(
            &FileUpdateObserver::OnEndUpdate, MakeTuple(*iter));
      }
    }
    write_target_urls_.erase(found);
  }
  DCHECK(operations_.Lookup(id));
  operations_.Remove(id);
}

}  // namespace fileapi
