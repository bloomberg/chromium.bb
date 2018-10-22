// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides File API related utilities.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "url/gurl.h"

class Profile;

namespace content {
struct FileChooserFileInfo;
class RenderFrameHost;
}

namespace storage {
class FileSystemContext;
}

namespace ui {
struct SelectedFileInfo;
}

namespace file_manager {
namespace util {

// Structure information necessary to create a EntryDefinition, and therefore
// an Entry object on the JavaScript side.
struct FileDefinition {
  base::FilePath virtual_path;
  base::FilePath absolute_path;
  bool is_directory;
};

// Contains all information needed to create an Entry object in custom bindings.
struct EntryDefinition {
  EntryDefinition();
  EntryDefinition(const EntryDefinition& other);
  ~EntryDefinition();

  std::string file_system_root_url;  // Used to create DOMFileSystem.
  std::string file_system_name;      // Value of DOMFileSystem.name.
  base::FilePath full_path;    // Value of Entry.fullPath.
  // Whether to create FileEntry or DirectoryEntry when the corresponding entry
  // is not found.
  bool is_directory;
  base::File::Error error;
};

typedef std::vector<FileDefinition> FileDefinitionList;
typedef std::vector<EntryDefinition> EntryDefinitionList;
typedef std::vector<ui::SelectedFileInfo> SelectedFileInfoList;
typedef std::vector<content::FileChooserFileInfo> FileChooserFileInfoList;

// The callback used by ConvertFileDefinitionToEntryDefinition. Returns the
// result of the conversion.
typedef base::Callback<void(const EntryDefinition& entry_definition)>
    EntryDefinitionCallback;

// The callback used by ConvertFileDefinitionListToEntryDefinitionList. Returns
// the result of the conversion as a list.
typedef base::Callback<void(
    std::unique_ptr<EntryDefinitionList> entry_definition_list)>
    EntryDefinitionListCallback;

// The callback used by
// ConvertFileSelectedInfoListToFileChooserFileInfoList. Returns the result of
// the conversion as a list.
typedef base::Callback<void(const FileChooserFileInfoList&)>
    FileChooserFileInfoListCallback;

// Returns a file system context associated with the given profile and the
// extension ID.
storage::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id);

// Returns a file system context associated with the given profile and the
// render view host.
storage::FileSystemContext* GetFileSystemContextForRenderFrameHost(
    Profile* profile,
    content::RenderFrameHost* render_frame_host);

// Converts DrivePath (e.g., "drive/root", which always starts with the fixed
// "drive" directory) to a RelativeFileSystemPathrelative (e.g.,
// "drive-xxx/root/foo". which starts from the "mount point" in the FileSystem
// API that may be distinguished for each profile by the appended "xxx" part.)
base::FilePath ConvertDrivePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& drive_path);

// Converts DrivePath to FileSystem URL.
// E.g., "drive/root" to filesystem://id/external/drive-xxx/root.
GURL ConvertDrivePathToFileSystemUrl(Profile* profile,
                                     const base::FilePath& drive_path,
                                     const std::string& extension_id);

// Converts AbsolutePath (e.g., "/special/drive-xxx/root" or
// "/home/chronos/u-xxx/Downloads") into filesystem URL. Returns false
// if |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToFileSystemUrl(Profile* profile,
                                            const base::FilePath& absolute_path,
                                            const std::string& extension_id,
                                            GURL* url);

// Converts AbsolutePath into RelativeFileSystemPath (e.g.,
// "/special/drive-xxx/root/foo" => "drive-xxx/root/foo".) Returns false if
// |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* relative_path);

// Converts a file definition to a entry definition and returns the result
// via a callback. |profile| cannot be null. Must be called on UI thread.
void ConvertFileDefinitionToEntryDefinition(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinition& file_definition,
    const EntryDefinitionCallback& callback);

// Converts a list of file definitions into a list of entry definitions and
// returns it via |callback|. The method is safe, |file_definition_list| is
// copied internally. The output list has the same order of items and size as
// the input vector. |profile| cannot be null. Must be called on UI thread.
void ConvertFileDefinitionListToEntryDefinitionList(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinitionList& file_definition_list,
    const EntryDefinitionListCallback& callback);

// Converts SelectedFileInfoList into FileChooserFileInfoList.
void ConvertSelectedFileInfoListToFileChooserFileInfoList(
    storage::FileSystemContext* context,
    const GURL& origin,
    const SelectedFileInfoList& selected_info_list,
    const FileChooserFileInfoListCallback& callback);

// Checks if a directory exists at |directory_path| absolute path.
void CheckIfDirectoryExists(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& directory_path,
    const storage::FileSystemOperationRunner::StatusCallback& callback);

// Get metadata for an entry at |entry_path| absolute path.
void GetMetadataForPath(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& entry_path,
    int fields,
    const storage::FileSystemOperationRunner::GetMetadataCallback& callback);

// Obtains isolated file system URL from |virtual_path| pointing a file in the
// external file system.
storage::FileSystemURL CreateIsolatedURLFromVirtualPath(
    const storage::FileSystemContext& context,
    const GURL& origin,
    const base::FilePath& virtual_path);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILEAPI_UTIL_H_
