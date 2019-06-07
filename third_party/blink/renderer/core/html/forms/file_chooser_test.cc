// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/mock_file_chooser.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"

namespace blink {

class FileChooserTest : public testing::Test {
 protected:
  void SetUp() override { web_view_ = helper_.Initialize(); }

  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
};

TEST_F(FileChooserTest, NotGarbageCollected) {
  LocalFrame* frame = helper_.LocalMainFrame()->GetFrame();
  auto* client = MakeGarbageCollected<MockFileChooserClient>(frame);
  mojom::blink::FileChooserParams params;
  params.title = g_empty_string;
  scoped_refptr<FileChooser> chooser = FileChooser::Create(client, params);
  ThreadState::Current()->CollectAllGarbageForTesting();
  // This tests the nullness of FileChooser::client_.
  EXPECT_TRUE(chooser->FrameOrNull());
}

}  // namespace blink
