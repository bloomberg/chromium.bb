// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_HANDLERS_NON_NATIVE_FILE_SYSTEM_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_FILE_HANDLERS_NON_NATIVE_FILE_SYSTEM_DELEGATE_H_

#include "base/callback.h"
#include "base/files/file_path.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Delegate for extension APIs to delegate tasks to a non-native file system on
// Chrome OS.
class NonNativeFileSystemDelegate {
 public:
  virtual ~NonNativeFileSystemDelegate() {}

  // Checks whether |path| points to a non-local filesystem that
  // requires special handling.
  virtual bool IsUnderNonNativeLocalPath(content::BrowserContext* context,
                                         const base::FilePath& path) = 0;

  // Returns the mime type of the file pointed by |path|, and asynchronously
  // sends the result to |callback|.
  virtual void GetNonNativeLocalPathMimeType(
      content::BrowserContext* context,
      const base::FilePath& path,
      const base::Callback<void(bool, const std::string&)>& callback) = 0;

  // Checks whether |path| points to a non-local filesystem directory and calls
  // |callback| with the result asynchronously.
  virtual void IsNonNativeLocalPathDirectory(
      content::BrowserContext* context,
      const base::FilePath& path,
      const base::Callback<void(bool)>& callback) = 0;

  // Ensures a non-local file exists at |path|, i.e., it does nothing if a file
  // is already present, or creates a file there if it isn't. Asynchronously
  // calls |callback| with a success value.
  virtual void PrepareNonNativeLocalFileForWritableApp(
      content::BrowserContext* context,
      const base::FilePath& path,
      const base::Callback<void(bool)>& callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_HANDLERS_NON_NATIVE_FILE_SYSTEM_DELEGATE_H_
