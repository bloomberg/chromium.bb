// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"

namespace fileapi {

enum FileSystemType {
  // Indicates uninitialized or invalid filesystem type.
  kFileSystemTypeUnknown = -1,

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

  // Should be used only for testing.
  kFileSystemTypeTest = 100,

  // Following file system types are internal and they are not exposed to
  // WebKit, but are accessible via IsolatedContext.

  // Indicates a transient, isolated file system for a native local path.
  // TODO(kinuko): Rename all kFileSystemTypeIsolated used as internal type
  // with this one.
  kFileSystemTypeNativeLocal,

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
};

enum FileSystemMountType {
  kFileSystemMountTypeUnknown = -1,

  // For kFileSystemTypeIsolated file systems. URLs for this type of
  // file system is cracked via IsolatedContext.
  kFileSystemMountTypeIsolated = kFileSystemTypeIsolated,

  // For kFileSystemTypeIsolated file systems. URLs for this type of
  // file system is cracked via IsolatedContext.
  kFileSystemMountTypeExternal = kFileSystemTypeExternal,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_TYPES_H_
