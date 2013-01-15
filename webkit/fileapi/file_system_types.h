// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"

namespace fileapi {

enum FileSystemType {
  // Indicates uninitialized or invalid filesystem type.
  kFileSystemTypeUnknown = -1,

  // ------------------------------------------------------------------------
  // Public FileSystem types, that are embedded in filesystem: URL and exposed
  // to WebKit/renderer. Both Chrome and WebKit know how to handle these types.

  // Following two types are for TEMPORARY or PERSISTENT filesystems that
  // can be used by webapps via standard app-facing API
  // as defined in File API: Directories and System.
  // http://www.w3.org/TR/file-system-api/#temporary-vs.-persistent-storage
  // They are sandboxed filesystems; all the files in the filesystems are
  // placed under the profile directory with path obfuscation and quota
  // enforcement.
  kFileSystemTypeTemporary = WebKit::WebFileSystem::TypeTemporary,
  kFileSystemTypePersistent = WebKit::WebFileSystem::TypePersistent,

  // Indicates non-sandboxed isolated filesystem.
  kFileSystemTypeIsolated = WebKit::WebFileSystem::TypeIsolated,

  // Indicates non-sandboxed filesystem where files are placed outside the
  // profile directory (thus called 'external' filesystem).
  // This filesystem is used only by Chrome OS as of writing.
  kFileSystemTypeExternal = WebKit::WebFileSystem::TypeExternal,

  // ------------------------------------------------------------------------
  // Private FileSystem types, that should not appear in filesystem: URL as
  // WebKit has no idea how to handle those types.
  //
  // One can register (mount) a new file system with a private file system type
  // using IsolatedContext.  Files in such file systems can be accessed via
  // either Isolated or External public file system types (depending on
  // how the file system is registered).
  // See the comments for IsolatedContext and/or FileSystemURL for more details.

  // Should be used only for testing.
  kFileSystemTypeTest = 100,

  // Indicates a local filesystem where we can access files using native
  // local path.
  kFileSystemTypeNativeLocal,

  // Indicates a local filesystem where we can access files using native
  // local path, but with restricted access.
  // Restricted native local file system is in read-only mode.
  kFileSystemTypeRestrictedNativeLocal,

  // Indicates a transient, isolated file system for dragged files (which could
  // contain multiple dragged paths in the virtual root).
  kFileSystemTypeDragged,

  // Indicates media filesystem which we can access with same manner to
  // regular filesystem.
  kFileSystemTypeNativeMedia,

  // Indicates media filesystem to which we need special protocol to access,
  // such as MTP or PTP.
  kFileSystemTypeDeviceMedia,

  // Indicates a Drive filesystem which provides access to Google Drive.
  kFileSystemTypeDrive,

  // Indicates a Syncable sandboxed filesystem which can be backed by a
  // cloud storage service.
  kFileSystemTypeSyncable,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_
