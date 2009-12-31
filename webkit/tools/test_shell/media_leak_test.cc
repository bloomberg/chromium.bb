// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"

class MediaLeakTest : public TestShellTest {
};

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))

// This test plays a Theora video file for 1 second. It tries to expose
// memory leaks during a normal playback.
TEST_F(MediaLeakTest, VideoBear) {
  FilePath media_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &media_file));
  media_file = media_file.Append(FILE_PATH_LITERAL("webkit"))
                         .Append(FILE_PATH_LITERAL("data"))
                         .Append(FILE_PATH_LITERAL("media"))
                         .Append(FILE_PATH_LITERAL("bear.html"));
  test_shell_->LoadFile(media_file);
  test_shell_->WaitTestFinished();
}

// This test loads a Theora video file and unloads it many times. It tries
// to expose memory leaks in the glue layer with WebKit.
TEST_F(MediaLeakTest, DISABLED_ManyVideoBear) {
  FilePath media_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &media_file));
  media_file = media_file.Append(FILE_PATH_LITERAL("webkit"))
                         .Append(FILE_PATH_LITERAL("data"))
                         .Append(FILE_PATH_LITERAL("media"))
                         .Append(FILE_PATH_LITERAL("manybear.html"));
  test_shell_->LoadFile(media_file);
  test_shell_->WaitTestFinished();
}

#endif
