// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_data_directory.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

namespace net {

namespace {

// Net data directory, relative to source root.
const base::FilePath::CharType kNetDataRelativePath[] =
    FILE_PATH_LITERAL("net/data");

// Test certificates directory, relative to kNetDataRelativePath.
const base::FilePath::CharType kCertificateDataSubPath[] =
    FILE_PATH_LITERAL("ssl/certificates");

}  // namespace

base::FilePath GetTestNetDataDirectory() {
  base::FilePath src_root;
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_path_service;
    PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  }

  return src_root.Append(kNetDataRelativePath);
}

base::FilePath GetTestCertsDirectory() {
  return GetTestNetDataDirectory().Append(kCertificateDataSubPath);
}

base::FilePath GetTestClientCertsDirectory() {
#if defined(OS_ANDROID)
  return base::FilePath(kNetDataRelativePath).Append(kCertificateDataSubPath);
#else
  return GetTestCertsDirectory();
#endif
}

base::FilePath GetWebSocketTestDataDirectory() {
  base::FilePath data_dir(FILE_PATH_LITERAL("net/data/websocket"));
  return data_dir;
}

}  // namespace net
