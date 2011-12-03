// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"

using WebKit::WebString;

namespace webkit {
namespace ppapi {

TEST(PPB_FileChooser_ImplTest, ParseAcceptValue) {
  std::vector<WebString> parsed;

  parsed = PPB_FileChooser_Impl::ParseAcceptValue("");
  EXPECT_EQ(0U, parsed.size());

  parsed = PPB_FileChooser_Impl::ParseAcceptValue(" ");
  EXPECT_EQ(0U, parsed.size());

  parsed = PPB_FileChooser_Impl::ParseAcceptValue("a");
  EXPECT_EQ(0U, parsed.size());

  parsed = PPB_FileChooser_Impl::ParseAcceptValue(",, ");
  EXPECT_EQ(0U, parsed.size());

  parsed = PPB_FileChooser_Impl::ParseAcceptValue(
    "IMAGE/*,,!!,,text/plain, audio /(*) ");
  EXPECT_EQ(3U, parsed.size());
  EXPECT_EQ("image/*", parsed[0]);
  EXPECT_EQ("text/plain", parsed[1]);
  // We don't need to reject invalid MIME tokens strictly.
  EXPECT_EQ("audio /(*)", parsed[2]);
}

}  // namespace ppapi
}  // namespace webkit
