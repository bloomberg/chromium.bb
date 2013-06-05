// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_operation_runner.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace fileapi {

typedef FileSystemOperationRunner::OperationID OperationID;

const OperationID FileSystemOperationRunner::kErrorOperationID = -1;

FileSystemOperationRunner::~FileSystemOperationRunner() {
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
    callback.Run(error, base::PlatformFileInfo(), base::FilePath());
    return kErrorOperationID;
  }
  OperationID id = operations_.Add(operation);
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
  OperationID id = operations_.Add(operation);
  operation->Write(
      url_request_context, url, blob_url, offset,
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
  operation->CreateSnapshotFile(
      url,
      base::Bind(&FileSystemOperationRunner::DidCreateSnapshot, AsWeakPtr(),
                 id, callback));
  return id;
}

FileSystemOperationRunner::FileSystemOperationRunner(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {}

void FileSystemOperationRunner::DidFinish(
    OperationID id,
    const StatusCallback& callback,
    base::PlatformFileError rv) {
  callback.Run(rv);
  DCHECK(operations_.Lookup(id));
  operations_.Remove(id);
}

void FileSystemOperationRunner::DidGetMetadata(
    OperationID id,
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  callback.Run(rv, file_info, platform_path);
  DCHECK(operations_.Lookup(id));
  operations_.Remove(id);
}

void FileSystemOperationRunner::DidReadDirectory(
    OperationID id,
    const ReadDirectoryCallback& callback,
    base::PlatformFileError rv,
    const std::vector<DirectoryEntry>& entries,
    bool has_more) {
  callback.Run(rv, entries, has_more);
  if (rv != base::PLATFORM_FILE_OK || !has_more) {
    DCHECK(operations_.Lookup(id));
    operations_.Remove(id);
  }
}

void FileSystemOperationRunner::DidWrite(
    OperationID id,
    const WriteCallback& callback,
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  callback.Run(rv, bytes, complete);
  if (rv != base::PLATFORM_FILE_OK || complete) {
    DCHECK(operations_.Lookup(id));
    operations_.Remove(id);
  }
}

void FileSystemOperationRunner::DidOpenFile(
    OperationID id,
    const OpenFileCallback& callback,
    base::PlatformFileError rv,
    base::PlatformFile file,
    const base::Closure& on_close_callback,
    base::ProcessHandle peer_handle) {
  callback.Run(rv, file, on_close_callback, peer_handle);
  DCHECK(operations_.Lookup(id));
  operations_.Remove(id);
}

void FileSystemOperationRunner::DidCreateSnapshot(
    OperationID id,
    const SnapshotFileCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  callback.Run(rv, file_info, platform_path, file_ref);
  DCHECK(operations_.Lookup(id));
  operations_.Remove(id);
}

}  // namespace fileapi
