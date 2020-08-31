// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_system_backend.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/fileapi/file_access_permissions.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/origin.h"

namespace chromeos {
namespace {

// TODO(mtomasz): Remove this hacky whitelist.
// See: crbug.com/271946
const char* kOemAccessibleExtensions[] = {
    "mlbmkoenclnokonejhlfakkeabdlmpek",  // TimeScapes,
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Retail Demo (public session),
    "klimoghijjogocdbaikffefjfcfheiel",  // Retail Demo (OOBE),
};

}  // namespace

// static
bool FileSystemBackend::CanHandleURL(const storage::FileSystemURL& url) {
  if (!url.is_valid())
    return false;
  return url.type() == storage::kFileSystemTypeNativeLocal ||
         url.type() == storage::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider ||
         url.type() == storage::kFileSystemTypeDriveFs ||
         url.type() == storage::kFileSystemTypeSmbFs;
}

FileSystemBackend::FileSystemBackend(
    std::unique_ptr<FileSystemBackendDelegate> file_system_provider_delegate,
    std::unique_ptr<FileSystemBackendDelegate> mtp_delegate,
    std::unique_ptr<FileSystemBackendDelegate> arc_content_delegate,
    std::unique_ptr<FileSystemBackendDelegate> arc_documents_provider_delegate,
    std::unique_ptr<FileSystemBackendDelegate> drivefs_delegate,
    std::unique_ptr<FileSystemBackendDelegate> smbfs_delegate,
    scoped_refptr<storage::ExternalMountPoints> mount_points,
    storage::ExternalMountPoints* system_mount_points)
    : file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(storage::AsyncFileUtil::CreateForLocalFileSystem()),
      file_system_provider_delegate_(std::move(file_system_provider_delegate)),
      mtp_delegate_(std::move(mtp_delegate)),
      arc_content_delegate_(std::move(arc_content_delegate)),
      arc_documents_provider_delegate_(
          std::move(arc_documents_provider_delegate)),
      drivefs_delegate_(std::move(drivefs_delegate)),
      smbfs_delegate_(std::move(smbfs_delegate)),
      mount_points_(mount_points),
      system_mount_points_(system_mount_points) {}

FileSystemBackend::~FileSystemBackend() {
}

void FileSystemBackend::AddSystemMountPoints() {
  // RegisterFileSystem() is no-op if the mount point with the same name
  // already exists, hence it's safe to call without checking if a mount
  // point already exists or not.
  system_mount_points_->RegisterFileSystem(
      kSystemMountNameArchive, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(),
      chromeos::CrosDisksClient::GetArchiveMountPoint());
  system_mount_points_->RegisterFileSystem(
      kSystemMountNameRemovable, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(storage::FlushPolicy::FLUSH_ON_COMPLETION),
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint());
  system_mount_points_->RegisterFileSystem(
      kSystemMountNameOem, storage::kFileSystemTypeRestrictedNativeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(FILE_PATH_LITERAL("/usr/share/oem")));
}

bool FileSystemBackend::CanHandleType(storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeExternal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeNativeForPlatformApp:
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::kFileSystemTypeProvided:
    case storage::kFileSystemTypeArcContent:
    case storage::kFileSystemTypeArcDocumentsProvider:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
      return true;
    default:
      return false;
  }
}

void FileSystemBackend::Initialize(storage::FileSystemContext* context) {
}

void FileSystemBackend::ResolveURL(const storage::FileSystemURL& url,
                                   storage::OpenFileSystemMode mode,
                                   OpenFileSystemCallback callback) {
  std::string id;
  storage::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  storage::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(
           url.virtual_path(), &id, &type, &cracked_id, &path, &option) &&
      !system_mount_points_->CrackVirtualPath(
           url.virtual_path(), &id, &type, &cracked_id, &path, &option)) {
    // Not under a mount point, so return an error, since the root is not
    // accessible.
    GURL root_url = GURL(storage::GetExternalFileSystemRootURIString(
        url.origin().GetURL(), std::string()));
    std::move(callback).Run(root_url, std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  std::string name;
  // Construct a URL restricted to the found mount point.
  std::string root_url =
      storage::GetExternalFileSystemRootURIString(url.origin().GetURL(), id);

  // For removable and archives, the file system root is the external mount
  // point plus the inner mount point.
  if (id == "archive" || id == "removable") {
    std::vector<std::string> components;
    url.virtual_path().GetComponents(&components);
    DCHECK_EQ(id, components.at(0));
    if (components.size() < 2) {
      // Unable to access /archive and /removable directories directly. The
      // inner mount name must be specified.
      std::move(callback).Run(GURL(root_url), std::string(),
                              base::File::FILE_ERROR_SECURITY);
      return;
    }
    std::string inner_mount_name = components[1];
    root_url += inner_mount_name + "/";
    name = inner_mount_name;
  } else if (id == arc::kDocumentsProviderMountPointName) {
    // For ARC documents provider file system, volumes are mounted per document
    // provider root, so we need to fix up |root_url| to point to an individual
    // root.
    std::string authority;
    std::string root_document_id;
    base::FilePath unused_path;
    if (!arc::ParseDocumentsProviderUrl(url, &authority, &root_document_id,
                                        &unused_path)) {
      std::move(callback).Run(GURL(root_url), std::string(),
                              base::File::FILE_ERROR_SECURITY);
      return;
    }
    base::FilePath mount_path =
        arc::GetDocumentsProviderMountPath(authority, root_document_id);
    base::FilePath relative_mount_path;
    base::FilePath(arc::kDocumentsProviderMountPointPath)
        .AppendRelativePath(mount_path, &relative_mount_path);
    root_url +=
        net::EscapePath(storage::FilePathToString(relative_mount_path)) + "/";
    name = authority + ":" + root_document_id;
  } else {
    name = id;
  }

  std::move(callback).Run(GURL(root_url), name, base::File::FILE_OK);
}

storage::FileSystemQuotaUtil* FileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

const storage::UpdateObserverList* FileSystemBackend::GetUpdateObservers(
    storage::FileSystemType type) const {
  return NULL;
}

const storage::ChangeObserverList* FileSystemBackend::GetChangeObservers(
    storage::FileSystemType type) const {
  return NULL;
}

const storage::AccessObserverList* FileSystemBackend::GetAccessObservers(
    storage::FileSystemType type) const {
  return NULL;
}

bool FileSystemBackend::IsAccessAllowed(
    const storage::FileSystemURL& url) const {
  if (!url.is_valid())
    return false;

  // No extra check is needed for isolated file systems.
  if (url.mount_type() == storage::kFileSystemTypeIsolated)
    return true;

  if (!CanHandleURL(url))
    return false;

  // If there is no origin set, then it's an internal access.
  if (url.origin().opaque())
    return true;

  const std::string& extension_id = url.origin().host();
  if (url.type() == storage::kFileSystemTypeRestrictedNativeLocal) {
    for (size_t i = 0; i < base::size(kOemAccessibleExtensions); ++i) {
      if (extension_id == kOemAccessibleExtensions[i])
        return true;
    }
  }

  return file_access_permissions_->HasAccessPermission(extension_id,
                                                       url.virtual_path());
}

void FileSystemBackend::GrantFileAccessToExtension(
    const std::string& extension_id, const base::FilePath& virtual_path) {
  std::string id;
  storage::FileSystemType type;
  std::string cracked_id;
  base::FilePath path;
  storage::FileSystemMountOption option;
  if (!mount_points_->CrackVirtualPath(virtual_path, &id, &type, &cracked_id,
                                       &path, &option) &&
      !system_mount_points_->CrackVirtualPath(virtual_path, &id, &type,
                                              &cracked_id, &path, &option)) {
    return;
  }

  file_access_permissions_->GrantAccessPermission(extension_id, virtual_path);
}

void FileSystemBackend::RevokeAccessForExtension(
      const std::string& extension_id) {
  file_access_permissions_->RevokePermissions(extension_id);
}

std::vector<base::FilePath> FileSystemBackend::GetRootDirectories() const {
  std::vector<storage::MountPoints::MountPointInfo> mount_points;
  mount_points_->AddMountPointInfosTo(&mount_points);
  system_mount_points_->AddMountPointInfosTo(&mount_points);

  std::vector<base::FilePath> root_dirs;
  for (size_t i = 0; i < mount_points.size(); ++i)
    root_dirs.push_back(mount_points[i].path);
  return root_dirs;
}

storage::AsyncFileUtil* FileSystemBackend::GetAsyncFileUtil(
    storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
      return local_file_util_.get();
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeArcContent:
      return arc_content_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeDriveFs:
      return drivefs_delegate_->GetAsyncFileUtil(type);
    case storage::kFileSystemTypeSmbFs:
      return smbfs_delegate_->GetAsyncFileUtil(type);
    default:
      NOTREACHED();
  }
  return NULL;
}

storage::WatcherManager* FileSystemBackend::GetWatcherManager(
    storage::FileSystemType type) {
  if (type == storage::kFileSystemTypeProvided)
    return file_system_provider_delegate_->GetWatcherManager(type);

  if (type == storage::kFileSystemTypeDeviceMediaAsFileStorage) {
    return mtp_delegate_->GetWatcherManager(type);
  }

  if (type == storage::kFileSystemTypeArcDocumentsProvider)
    return arc_documents_provider_delegate_->GetWatcherManager(type);

  // TODO(mtomasz): Add support for other backends.
  return NULL;
}

storage::CopyOrMoveFileValidatorFactory*
FileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    storage::FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return NULL;
}

storage::FileSystemOperation* FileSystemBackend::CreateFileSystemOperation(
    const storage::FileSystemURL& url,
    storage::FileSystemContext* context,
    base::File::Error* error_code) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return NULL;
  }

  if (url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage) {
    // MTP file operations run on MediaTaskRunner.
    return storage::FileSystemOperation::Create(
        url, context,
        std::make_unique<storage::FileSystemOperationContext>(
            context, MediaFileSystemBackend::MediaTaskRunner().get()));
  }
  if (url.type() == storage::kFileSystemTypeNativeLocal ||
      url.type() == storage::kFileSystemTypeRestrictedNativeLocal ||
      url.type() == storage::kFileSystemTypeDriveFs ||
      url.type() == storage::kFileSystemTypeSmbFs) {
    return storage::FileSystemOperation::Create(
        url, context,
        std::make_unique<storage::FileSystemOperationContext>(
            context, base::ThreadPool::CreateSequencedTaskRunner(
                         {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
                         .get()));
  }

  DCHECK(url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider);
  return storage::FileSystemOperation::Create(
      url, context,
      std::make_unique<storage::FileSystemOperationContext>(context));
}

bool FileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  return url.type() == storage::kFileSystemTypeProvided ||
         url.type() == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         url.type() == storage::kFileSystemTypeArcContent ||
         url.type() == storage::kFileSystemTypeArcDocumentsProvider;
}

bool FileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeProvided:
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::kFileSystemTypeDriveFs:
      return true;
    // TODO(fukino): Support in-place copy for DocumentsProvider.
    // crbug.com/953603.
    case storage::kFileSystemTypeArcDocumentsProvider:
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeArcContent:
    // TODO(crbug.com/939235): Implement in-place copy in SmbFs.
    case storage::kFileSystemTypeSmbFs:
      return false;
    default:
      NOTREACHED();
  }
  return true;
}

std::unique_ptr<storage::FileStreamReader>
FileSystemBackend::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url))
    return std::unique_ptr<storage::FileStreamReader>();

  switch (url.type()) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
      return std::unique_ptr<storage::FileStreamReader>(
          storage::FileStreamReader::CreateForLocalFile(
              base::ThreadPool::CreateTaskRunner(
                  {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
                  .get(),
              url.path(), offset, expected_modification_time));
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    case storage::kFileSystemTypeArcContent:
      return arc_content_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->CreateFileStreamReader(
          url, offset, max_bytes_to_read, expected_modification_time, context);
    default:
      NOTREACHED();
  }
  return std::unique_ptr<storage::FileStreamReader>();
}

std::unique_ptr<storage::FileStreamWriter>
FileSystemBackend::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url))
    return std::unique_ptr<storage::FileStreamWriter>();

  switch (url.type()) {
    case storage::kFileSystemTypeProvided:
      return file_system_provider_delegate_->CreateFileStreamWriter(
          url, offset, context);
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
      return storage::FileStreamWriter::CreateForLocalFile(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
              .get(),
          url.path(), offset, storage::FileStreamWriter::OPEN_EXISTING_FILE);
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      return mtp_delegate_->CreateFileStreamWriter(url, offset, context);
    case storage::kFileSystemTypeArcDocumentsProvider:
      return arc_documents_provider_delegate_->CreateFileStreamWriter(
          url, offset, context);
    // Read only file systems.
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeArcContent:
      return std::unique_ptr<storage::FileStreamWriter>();
    default:
      NOTREACHED();
  }
  return std::unique_ptr<storage::FileStreamWriter>();
}

bool FileSystemBackend::GetVirtualPath(const base::FilePath& filesystem_path,
                                       base::FilePath* virtual_path) const {
  return mount_points_->GetVirtualPath(filesystem_path, virtual_path) ||
         system_mount_points_->GetVirtualPath(filesystem_path, virtual_path);
}

void FileSystemBackend::GetRedirectURLForContents(
    const storage::FileSystemURL& url,
    storage::URLCallback callback) const {
  DCHECK(url.is_valid());

  if (!IsAccessAllowed(url)) {
    std::move(callback).Run(GURL());
    return;
  }

  switch (url.type()) {
    case storage::kFileSystemTypeProvided:
      file_system_provider_delegate_->GetRedirectURLForContents(
          url, std::move(callback));
      return;
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
      mtp_delegate_->GetRedirectURLForContents(url, std::move(callback));
      return;
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeArcContent:
    case storage::kFileSystemTypeArcDocumentsProvider:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
      std::move(callback).Run(GURL());
      return;
    default:
      NOTREACHED();
  }
  std::move(callback).Run(GURL());
}

storage::FileSystemURL FileSystemBackend::CreateInternalURL(
    storage::FileSystemContext* context,
    const base::FilePath& entry_path) const {
  base::FilePath virtual_path;
  if (!GetVirtualPath(entry_path, &virtual_path))
    return storage::FileSystemURL();

  return context->CreateCrackedFileSystemURL(
      url::Origin(), storage::kFileSystemTypeExternal, virtual_path);
}

}  // namespace chromeos
