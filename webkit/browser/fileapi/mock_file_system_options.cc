// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/mock_file_system_options.h"

#include <string>
#include <vector>

namespace fileapi {

FileSystemOptions CreateIncognitoFileSystemOptions() {
  std::vector<std::string> additional_allowed_schemes;
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                           additional_allowed_schemes);
};

FileSystemOptions CreateAllowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back("file");
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes);
};

FileSystemOptions CreateDisallowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes);
};

}  // namespace fileapi
