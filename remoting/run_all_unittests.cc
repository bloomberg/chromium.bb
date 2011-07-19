// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/test_suite.h"
#include "media/base/media.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // Load the media library so we can use libvpx.
  FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  CHECK(media::InitializeMediaLibrary(path))
      << "Cannot load media library";
  return test_suite.Run();
}
