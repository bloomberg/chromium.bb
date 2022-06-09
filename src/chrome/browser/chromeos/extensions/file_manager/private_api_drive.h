// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Drive specific API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "base/files/file.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/logged_extension_function.h"
#include "components/drive/file_errors.h"

namespace google_apis {
class AuthService;
}

namespace extensions {

namespace api {
namespace file_manager_private {
struct EntryProperties;
}  // namespace file_manager_private
}  // namespace api

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (base::File::Error).
class FileManagerPrivateInternalGetEntryPropertiesFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getEntryProperties",
                             FILEMANAGERPRIVATEINTERNAL_GETENTRYPROPERTIES)

  FileManagerPrivateInternalGetEntryPropertiesFunction();

 protected:
  ~FileManagerPrivateInternalGetEntryPropertiesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void CompleteGetEntryProperties(
      size_t index,
      const storage::FileSystemURL& url,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  size_t processed_count_;
  std::vector<api::file_manager_private::EntryProperties> properties_list_;
};

// Implements the chrome.fileManagerPrivate.pinDriveFile method.
class FileManagerPrivateInternalPinDriveFileFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalPinDriveFileFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.pinDriveFile",
                             FILEMANAGERPRIVATEINTERNAL_PINDRIVEFILE)

 protected:
  ~FileManagerPrivateInternalPinDriveFileFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ResponseAction RunAsyncForDriveFs(
      const storage::FileSystemURL& file_system_url,
      bool pin);

  // Callback for and RunAsyncForDriveFs().
  void OnPinStateSet(drive::FileError error);
};

class FileManagerPrivateSearchDriveFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateSearchDriveFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDrive",
                             FILEMANAGERPRIVATE_SEARCHDRIVE)

 protected:
  ~FileManagerPrivateSearchDriveFunction() override = default;

  ResponseAction Run() override;

 private:
  void OnSearchDriveFs(std::unique_ptr<base::ListValue> results);

  base::TimeTicks operation_start_;
  bool is_offline_;
};

// Similar to FileManagerPrivateSearchDriveFunction but this one is used for
// searching drive metadata which is stored locally.
class FileManagerPrivateSearchDriveMetadataFunction
    : public LoggedExtensionFunction {
 public:
  enum class SearchType {
    kText,
    kSharedWithMe,
    kOffline,
  };

  FileManagerPrivateSearchDriveMetadataFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDriveMetadata",
                             FILEMANAGERPRIVATE_SEARCHDRIVEMETADATA)

 protected:
  ~FileManagerPrivateSearchDriveMetadataFunction() override = default;

  ResponseAction Run() override;

 private:
  void OnSearchDriveFs(const std::string& query_text,
                       std::unique_ptr<base::ListValue> results);

  base::TimeTicks operation_start_;
  SearchType search_type_;
  bool is_offline_;
};

// Implements the chrome.fileManagerPrivate.getDriveConnectionState method.
class FileManagerPrivateGetDriveConnectionStateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDriveConnectionState",
                             FILEMANAGERPRIVATE_GETDRIVECONNECTIONSTATE)

 protected:
  ~FileManagerPrivateGetDriveConnectionStateFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getDownloadUrl method.
class FileManagerPrivateInternalGetDownloadUrlFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetDownloadUrlFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getDownloadUrl",
                             FILEMANAGERPRIVATEINTERNAL_GETDOWNLOADURL)

 protected:
  ~FileManagerPrivateInternalGetDownloadUrlFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGotDownloadUrl(GURL download_url);

  // Callback with an |access_token|, called by
  // drive::DriveReadonlyTokenFetcher.
  void OnTokenFetched(google_apis::ApiErrorCode code,
                      const std::string& access_token);

  ResponseAction RunAsyncForDriveFs(
      const storage::FileSystemURL& file_system_url);
  void OnGotMetadata(drive::FileError error,
                     drivefs::mojom::FileMetadataPtr metadata);

 private:
  GURL download_url_;
  std::unique_ptr<google_apis::AuthService> auth_service_;
};

// Implements the chrome.fileManagerPrivate.notifyDriveDialogResult method.
class FileManagerPrivateNotifyDriveDialogResultFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.notifyDriveDialogResult",
                             FILEMANAGERPRIVATE_NOTIFYDRIVEDIALOGRESULT)

 protected:
  ~FileManagerPrivateNotifyDriveDialogResultFunction() override = default;

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
