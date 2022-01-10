// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/filesystem_api_util.h"

#include <memory>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/task_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"

namespace file_manager {
namespace util {
namespace {

void GetMimeTypeAfterGetMetadata(
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK || !metadata ||
      metadata->content_mime_type.empty()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(std::move(metadata->content_mime_type));
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It extracts
// the mime type from the passed metadata from a providing extension.
void GetMimeTypeAfterGetMetadataForProvidedFileSystem(
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    std::unique_ptr<ash::file_system_provider::EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK || !metadata->mime_type.get()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(*metadata->mime_type);
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It passes
// the returned mime type to the callback.
void GetMimeTypeAfterGetMimeTypeForArcContentFileSystem(
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    const absl::optional<std::string>& mime_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (mime_type.has_value()) {
    std::move(callback).Run(mime_type.value());
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

// Helper function to converts a callback that takes boolean value to that takes
// File::Error, by regarding FILE_OK as the only successful value.
void BoolCallbackAsFileErrorCallback(base::OnceCallback<void(bool)> callback,
                                     base::File::Error error) {
  return std::move(callback).Run(error == base::File::FILE_OK);
}

// Part of PrepareFileOnIOThread. It tries to create a new file if the given
// |url| is not already inhabited.
void PrepareFileAfterCheckExistOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::StatusCallback callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    std::move(callback).Run(error);
    return;
  }

  // Call with the second argument |exclusive| set to false, meaning that it
  // is not an error even if the file already exists (it can happen if the file
  // is created after the previous FileExists call and before this CreateFile.)
  //
  // Note that the preceding call to FileExists is necessary for handling
  // read only filesystems that blindly rejects handling CreateFile().
  file_system_context->operation_runner()->CreateFile(url, false,
                                                      std::move(callback));
}

// Checks whether a file exists at the given |url|, and try creating it if it
// is not already there.
void PrepareFileOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto* const operation_runner = file_system_context->operation_runner();
  operation_runner->FileExists(
      url, base::BindOnce(&PrepareFileAfterCheckExistOnIOThread,
                          std::move(file_system_context), url,
                          base::BindOnce(&BoolCallbackAsFileErrorCallback,
                                         std::move(callback))));
}

}  // namespace

bool IsNonNativeFileSystemType(storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeRestrictedLocal:
    case storage::kFileSystemTypeDriveFs:
    case storage::kFileSystemTypeSmbFs:
      return false;
    default:
      // The path indeed corresponds to a mount point not associated with a
      // native local path.
      return true;
  }
}

bool IsUnderNonNativeLocalPath(Profile* profile,
                        const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, util::GetFileManagerURL(), &url)) {
    return false;
  }

  storage::FileSystemURL filesystem_url =
      GetFileSystemContextForSourceURL(profile, GetFileManagerURL())
          ->CrackURLInFirstPartyContext(url);
  if (!filesystem_url.is_valid())
    return false;

  return IsNonNativeFileSystemType(filesystem_url.type());
}

bool HasNonNativeMimeTypeProvider(Profile* profile,
                                  const base::FilePath& path) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  return (drive_integration_service &&
          drive_integration_service->GetMountPointPath().IsParent(path)) ||
         IsUnderNonNativeLocalPath(profile, path);
}

void GetNonNativeLocalPathMimeType(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(const absl::optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(HasNonNativeMimeTypeProvider(profile, path));

  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  base::FilePath drive_relative_path;
  if (drive_integration_service &&
      drive_integration_service->GetRelativeDrivePath(path,
                                                      &drive_relative_path)) {
    if (auto* drivefs = drive_integration_service->GetDriveFsInterface()) {
      drivefs->GetMetadata(
          drive_relative_path,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&GetMimeTypeAfterGetMetadata, std::move(callback)),
              drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));

      return;
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  if (ash::file_system_provider::util::IsFileSystemProviderLocalPath(path)) {
    ash::file_system_provider::util::LocalPathParser parser(profile, path);
    if (!parser.Parse()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
      return;
    }

    parser.file_system()->GetMetadata(
        parser.file_path(),
        ash::file_system_provider::ProvidedFileSystemInterface::
            METADATA_FIELD_MIME_TYPE,
        base::BindOnce(&GetMimeTypeAfterGetMetadataForProvidedFileSystem,
                       std::move(callback)));
    return;
  }

  if (arc::IsArcAllowedForProfile(profile) &&
      base::FilePath(arc::kContentFileSystemMountPointPath).IsParent(path)) {
    GURL arc_url = arc::PathToArcUrl(path);
    auto* runner =
        arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile);
    if (!runner) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
      return;
    }
    runner->GetMimeType(
        arc_url,
        base::BindOnce(&GetMimeTypeAfterGetMimeTypeForArcContentFileSystem,
                       std::move(callback)));
    return;
  }

  // We don't have a way to obtain metadata other than drive and FSP. Returns an
  // error with empty MIME type, that leads fallback guessing mime type from
  // file extensions.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
}

void IsNonNativeLocalPathDirectory(Profile* profile,
                                   const base::FilePath& path,
                                   base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  util::CheckIfDirectoryExists(
      GetFileManagerFileSystemContext(profile), path,
      base::BindOnce(&BoolCallbackAsFileErrorCallback, std::move(callback)));
}

void PrepareNonNativeLocalFileForWritableApp(
    Profile* profile,
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, util::GetFileManagerURL(), &url)) {
    // Posting to the current thread, so that we always call back asynchronously
    // independent from whether or not the operation succeeds.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  scoped_refptr<storage::FileSystemContext> const file_system_context =
      GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);
  const storage::FileSystemURL internal_url =
      backend->CreateInternalURL(file_system_context.get(), path);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrepareFileOnIOThread, file_system_context, internal_url,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

}  // namespace util
}  // namespace file_manager
