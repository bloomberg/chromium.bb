// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/deletable_file_reference.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webkit_blob {

TEST(DeletableFileReferenceTest, TestReferences) {
  scoped_refptr<base::MessageLoopProxy> loop_proxy =
      base::MessageLoopProxy::CreateForCurrentThread();

  // Create a file.
  FilePath file;
  file_util::CreateTemporaryFile(&file);
  EXPECT_TRUE(file_util::PathExists(file));

  // Create a first reference to that file.
  scoped_refptr<DeletableFileReference> reference1;
  reference1 = DeletableFileReference::Get(file);
  EXPECT_FALSE(reference1.get());
  reference1 = DeletableFileReference::GetOrCreate(file, loop_proxy);
  EXPECT_TRUE(reference1.get());
  EXPECT_TRUE(file == reference1->path());

  // Get a second reference to that file.
  scoped_refptr<DeletableFileReference> reference2;
  reference2 = DeletableFileReference::Get(file);
  EXPECT_EQ(reference1.get(), reference2.get());
  reference2 = DeletableFileReference::GetOrCreate(file, loop_proxy);
  EXPECT_EQ(reference1.get(), reference2.get());

  // Drop the first reference, the file and reference should still be there.
  reference1 = NULL;
  EXPECT_TRUE(DeletableFileReference::Get(file).get());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(file_util::PathExists(file));

  // Drop the second reference, the file and reference should get deleted.
  reference2 = NULL;
  EXPECT_FALSE(DeletableFileReference::Get(file).get());
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(file_util::PathExists(file));
}

}  // namespace webkit_blob
