// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_OPTIONS_H_
#define WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_OPTIONS_H_

#include "webkit/browser/fileapi/file_system_options.h"

namespace fileapi {

// Returns Filesystem options for incognito mode.
FileSystemOptions CreateIncognitoFileSystemOptions();

// Returns Filesystem options that allow file access.
FileSystemOptions CreateAllowFileAccessOptions();

// Returns Filesystem options that disallow file access.
FileSystemOptions CreateDisallowFileAccessOptions();

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_OPTIONS_H_
