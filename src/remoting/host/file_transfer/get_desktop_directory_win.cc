// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/get_desktop_directory.h"

#include <Windows.h>
#include <shlobj.h>

namespace remoting {

// We can't use PathService on Windows because it doesn't play nicely with
// impersonation. Even if we disable PathService's own cache, the Windows API
// itself does process-wide caching that can cause trouble on an impersonating
// thread. As such, we have to call the relevant API directly and be explicit
// about wanting impersonation handling.
protocol::FileTransferResult<base::FilePath> GetDesktopDirectory() {
  wchar_t buffer[MAX_PATH];
  buffer[0] = 0;
  // While passing NULL for the third parameter would normally get the directory
  // for the current user, there are process-wide caches that can cause trouble
  // when impersonation is in play, so specify the token explicitly.
  HRESULT hr =
      SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, GetCurrentThreadToken(),
                      SHGFP_TYPE_CURRENT, buffer);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get desktop directory: " << hr;
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR, hr);
  }

  return {kSuccessTag, buffer};
}

}  // namespace remoting
