// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace storage {

namespace {

size_t GetNumPathComponents(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  return components.size();
}

class LocalFileLockImpl : public FilesystemProxy::FileLock {
 public:
  LocalFileLockImpl(base::FilePath path, base::File lock)
      : path_(std::move(path)), lock_(std::move(lock)) {}
  ~LocalFileLockImpl() override {
    if (lock_.IsValid())
      Release();
  }

  // FilesystemProxy::FileLock implementation:
  base::File::Error Release() override {
    base::File::Error error = base::File::FILE_OK;
#if !defined(OS_FUCHSIA)
    error = lock_.Unlock();
#endif
    lock_.Close();
    FilesystemImpl::UnlockFileLocal(path_);
    return error;
  }

 private:
  const base::FilePath path_;
  base::File lock_;
};

class RemoteFileLockImpl : public FilesystemProxy::FileLock {
 public:
  explicit RemoteFileLockImpl(mojo::PendingRemote<mojom::FileLock> remote_lock)
      : remote_lock_(std::move(remote_lock)) {}
  ~RemoteFileLockImpl() override {
    if (remote_lock_)
      Release();
  }

  // FilesystemProxy::FileLock implementation:
  base::File::Error Release() override {
    DCHECK(remote_lock_);
    base::File::Error error = base::File::FILE_ERROR_IO;
    mojo::Remote<mojom::FileLock>(std::move(remote_lock_))->Release(&error);
    return error;
  }

 private:
  mojo::PendingRemote<mojom::FileLock> remote_lock_;
};

}  // namespace

FilesystemProxy::FilesystemProxy(decltype(UNRESTRICTED),
                                 const base::FilePath& root)
    : root_(root) {
  DCHECK(root_.IsAbsolute() || root_.empty());
}

FilesystemProxy::FilesystemProxy(
    decltype(RESTRICTED),
    const base::FilePath& root,
    mojo::PendingRemote<mojom::Directory> directory,
    scoped_refptr<base::SequencedTaskRunner> ipc_task_runner)
    : root_(root),
      num_root_components_(GetNumPathComponents(root_)),
      remote_directory_(std::move(directory)) {
  DCHECK(root_.IsAbsolute());
}

FilesystemProxy::~FilesystemProxy() = default;

bool FilesystemProxy::PathExists(const base::FilePath& path) {
  if (!remote_directory_)
    return base::PathExists(MaybeMakeAbsolute(path));

  bool exists = false;
  remote_directory_->PathExists(MakeRelative(path), &exists);
  return exists;
}

FileErrorOr<std::vector<base::FilePath>> FilesystemProxy::GetDirectoryEntries(
    const base::FilePath& path,
    DirectoryEntryType type) {
  const mojom::GetEntriesMode mode =
      type == DirectoryEntryType::kFilesOnly
          ? mojom::GetEntriesMode::kFilesOnly
          : mojom::GetEntriesMode::kFilesAndDirectories;
  if (!remote_directory_)
    return FilesystemImpl::GetDirectoryEntries(MaybeMakeAbsolute(path), mode);

  base::File::Error error = base::File::FILE_ERROR_IO;
  std::vector<base::FilePath> entries;
  remote_directory_->GetEntries(MakeRelative(path), mode, &error, &entries);
  if (error != base::File::FILE_OK)
    return error;

  // Fix up all the relative paths to be absolute.
  const base::FilePath root = path.IsAbsolute() ? path : root_.Append(path);
  for (auto& entry : entries)
    entry = root.Append(entry);
  return entries;
}

FileErrorOr<base::File> FilesystemProxy::OpenFile(const base::FilePath& path,
                                                  int flags) {
  if (!remote_directory_) {
    base::File file(MaybeMakeAbsolute(path), flags);
    if (!file.IsValid())
      return file.error_details();
    return file;
  }

  // NOTE: Remote directories only support a subset of |flags| values.
  const int kModeMask = base::File::FLAG_OPEN | base::File::FLAG_CREATE |
                        base::File::FLAG_OPEN_ALWAYS |
                        base::File::FLAG_CREATE_ALWAYS |
                        base::File::FLAG_OPEN_TRUNCATED;
  const int kWriteMask = base::File::FLAG_WRITE | base::File::FLAG_APPEND;
  const int kSupportedFlagsMask =
      kModeMask | kWriteMask | base::File::FLAG_READ;
  DCHECK((flags & ~kSupportedFlagsMask) == 0) << "Unsupported flags: " << flags;

  const int mode_flags = flags & kModeMask;
  mojom::FileOpenMode mode;
  switch (mode_flags) {
    case base::File::FLAG_OPEN:
      mode = mojom::FileOpenMode::kOpenIfExists;
      break;
    case base::File::FLAG_CREATE:
      mode = mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists;
      break;
    case base::File::FLAG_OPEN_ALWAYS:
      mode = mojom::FileOpenMode::kAlwaysOpen;
      break;
    case base::File::FLAG_CREATE_ALWAYS:
      mode = mojom::FileOpenMode::kAlwaysCreate;
      break;
    case base::File::FLAG_OPEN_TRUNCATED:
      mode = mojom::FileOpenMode::kOpenIfExistsAndTruncate;
      break;
    default:
      NOTREACHED() << "Invalid open mode flags: " << mode_flags;
      return base::File::FILE_ERROR_FAILED;
  }

  mojom::FileReadAccess read_access =
      (flags & base::File::FLAG_READ) != 0
          ? mojom::FileReadAccess::kReadAllowed
          : mojom::FileReadAccess::kReadNotAllowed;

  const int write_flags = flags & kWriteMask;
  mojom::FileWriteAccess write_access;
  switch (write_flags) {
    case 0:
      write_access = mojom::FileWriteAccess::kWriteNotAllowed;
      break;
    case base::File::FLAG_WRITE:
      write_access = mojom::FileWriteAccess::kWriteAllowed;
      break;
    case base::File::FLAG_APPEND:
      write_access = mojom::FileWriteAccess::kAppendOnly;
      break;
    default:
      NOTREACHED() << "Invalid write access flags: " << write_flags;
      return base::File::FILE_ERROR_FAILED;
  }

  base::File::Error error = base::File::FILE_ERROR_IO;
  base::File file;
  remote_directory_->OpenFile(MakeRelative(path), mode, read_access,
                              write_access, &error, &file);
  if (error != base::File::FILE_OK)
    return error;
  return file;
}

bool FilesystemProxy::RemoveFile(const base::FilePath& path) {
  if (!remote_directory_)
    return base::DeleteFile(MaybeMakeAbsolute(path), /*recursive=*/false);

  bool success = false;
  remote_directory_->RemoveFile(MakeRelative(path), &success);
  return success;
}

base::File::Error FilesystemProxy::CreateDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_) {
    if (!base::CreateDirectoryAndGetError(MaybeMakeAbsolute(path), &error))
      return error;
    return base::File::FILE_OK;
  }

  remote_directory_->CreateDirectory(MakeRelative(path), &error);
  return error;
}

bool FilesystemProxy::RemoveDirectory(const base::FilePath& path) {
  if (!remote_directory_) {
    const base::FilePath full_path = MaybeMakeAbsolute(path);
    if (!base::DirectoryExists(full_path))
      return false;
    return base::DeleteFile(full_path, /*recursive=*/false);
  }

  bool success = false;
  remote_directory_->RemoveDirectory(MakeRelative(path), &success);
  return success;
}

base::Optional<base::File::Info> FilesystemProxy::GetFileInfo(
    const base::FilePath& path) {
  if (!remote_directory_) {
    base::File::Info info;
    if (base::GetFileInfo(MaybeMakeAbsolute(path), &info))
      return info;
    return base::nullopt;
  }

  base::Optional<base::File::Info> info;
  remote_directory_->GetFileInfo(MakeRelative(path), &info);
  return info;
}

base::File::Error FilesystemProxy::RenameFile(const base::FilePath& old_path,
                                              const base::FilePath& new_path) {
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_) {
    if (!base::ReplaceFile(MaybeMakeAbsolute(old_path),
                           MaybeMakeAbsolute(new_path), &error)) {
      return error;
    }
    return base::File::FILE_OK;
  }

  remote_directory_->RenameFile(MakeRelative(old_path), MakeRelative(new_path),
                                &error);
  return error;
}

FileErrorOr<std::unique_ptr<FilesystemProxy::FileLock>>
FilesystemProxy::LockFile(const base::FilePath& path) {
  if (!remote_directory_) {
    base::FilePath full_path = MaybeMakeAbsolute(path);
    FileErrorOr<base::File> result = FilesystemImpl::LockFileLocal(full_path);
    if (result.is_error())
      return result.error();
    std::unique_ptr<FileLock> lock = std::make_unique<LocalFileLockImpl>(
        std::move(full_path), std::move(result.value()));
    return lock;
  }

  mojo::PendingRemote<mojom::FileLock> remote_lock;
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_->LockFile(MakeRelative(path), &error, &remote_lock))
    return error;
  if (error != base::File::FILE_OK)
    return error;

  std::unique_ptr<FileLock> lock =
      std::make_unique<RemoteFileLockImpl>(std::move(remote_lock));
  return lock;
}

base::FilePath FilesystemProxy::MakeRelative(const base::FilePath& path) const {
  DCHECK(remote_directory_);
  DCHECK(!path.ReferencesParent());

  // For a remote Directory, returned paths must always be relative to |root|.
  if (!path.IsAbsolute() || path.empty())
    return path;

  if (path == root_)
    return base::FilePath();

  // Absolute paths need to be rebased onto |root_|.
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  base::FilePath relative_path;
  for (size_t i = num_root_components_; i < components.size(); ++i)
    relative_path = relative_path.Append(components[i]);
  return relative_path;
}

base::FilePath FilesystemProxy::MaybeMakeAbsolute(
    const base::FilePath& path) const {
  DCHECK(!remote_directory_);
  if (path.IsAbsolute() || root_.empty())
    return path;

  return root_.Append(path);
}

}  // namespace storage
