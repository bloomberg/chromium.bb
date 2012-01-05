// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/mock_file_system_options.h"

namespace fileapi {

FileSystemOptions CreateIncognitoFileSystemOptions() {
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                           std::vector<std::string>());
};

FileSystemOptions CreateAllowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back("file");
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes);
};

FileSystemOptions CreateDisallowFileAccessOptions() {
  std::vector<std::string> additional_allowed_schemes;
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes);
};

}  // namespace fileapi
