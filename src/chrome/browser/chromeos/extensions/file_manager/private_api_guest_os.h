// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Guest OS API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_

#include "chrome/browser/chromeos/extensions/file_manager/logged_extension_function.h"
#include "chrome/common/extensions/api/file_manager_private.h"

namespace extensions {

// Implements the chrome.fileManagerPrivate.listMountableGuests method.
class FileManagerPrivateListMountableGuestsFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.listMountableGuests",
                             FILEMANAGERPRIVATE_LISTMOUNTABLEGUESTS)

 protected:
  ~FileManagerPrivateListMountableGuestsFunction() override = default;

 private:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_GUEST_OS_H_
