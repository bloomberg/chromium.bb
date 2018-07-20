// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class PreviewsResourceLoadingHintsTest : public PageTestBase {
 public:
  PreviewsResourceLoadingHintsTest() {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(1, 1));
  }

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(PreviewsResourceLoadingHintsTest, NoPatterns) {
  std::vector<WTF::String> subresources_to_block;

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), subresources_to_block);
  EXPECT_TRUE(hints->AllowLoad(KURL("https://www.example.com/")));
}

TEST_F(PreviewsResourceLoadingHintsTest, OnePattern) {
  std::vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back("foo.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example.com/"), true},
      {KURL("https://www.example.com/foo.js"), true},
      {KURL("https://www.example.com/foo.jpg"), false},
      {KURL("https://www.example.com/pages/foo.jpg"), false},
      {KURL("https://www.example.com/foobar.jpg"), true},
      {KURL("https://www.example.com/barfoo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg"), false},
      {KURL("http://www.example.com/foo.jpg?q=alpha"), false},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg"), true},
      {KURL("http://www.example.com/bar.jpg?q=foo.jpg#foo.jpg"), true},
  };

  for (const auto& test : tests)
    EXPECT_EQ(test.allow_load_expected, hints->AllowLoad(test.url));
}

TEST_F(PreviewsResourceLoadingHintsTest, MultiplePatterns) {
  std::vector<WTF::String> subresources_to_block;
  subresources_to_block.push_back(".example1.com/foo.jpg");
  subresources_to_block.push_back(".example1.com/bar.jpg");
  subresources_to_block.push_back(".example2.com/baz.jpg");

  PreviewsResourceLoadingHints* hints = PreviewsResourceLoadingHints::Create(
      dummy_page_holder_->GetDocument(), subresources_to_block);

  const struct {
    KURL url;
    bool allow_load_expected;
  } tests[] = {
      {KURL("https://www.example1.com/"), true},
      {KURL("https://www.example1.com/foo.js"), true},
      {KURL("https://www.example1.com/foo.jpg"), false},
      {KURL("https://www.example1.com/pages/foo.jpg"), true},
      {KURL("https://www.example1.com/foobar.jpg"), true},
      {KURL("https://www.example1.com/barfoo.jpg"), true},
      {KURL("http://www.example1.com/foo.jpg"), false},
      {KURL("http://www.example1.com/bar.jpg"), false},
      {KURL("http://www.example2.com/baz.jpg"), false},
      {KURL("http://www.example2.com/pages/baz.jpg"), true},
      {KURL("http://www.example2.com/baz.html"), true},
  };

  for (const auto& test : tests)
    EXPECT_EQ(test.allow_load_expected, hints->AllowLoad(test.url));
}

}  // namespace

}  // namespace blink
