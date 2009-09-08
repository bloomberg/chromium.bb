// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_lib.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)

// Test parsing a simple description: Real Audio.
TEST(MIMEDescriptionParse, Simple) {
  std::vector<WebPluginMimeType> types;
  ASSERT_TRUE(NPAPI::PluginLib::ParseMIMEDescription(
                  "audio/x-pn-realaudio-plugin:rpm:RealAudio document;",
                  &types));
  ASSERT_EQ(1U, types.size());
  const WebPluginMimeType& type = types[0];
  EXPECT_EQ("audio/x-pn-realaudio-plugin", type.mime_type);
  ASSERT_EQ(1U, type.file_extensions.size());
  EXPECT_EQ("rpm", type.file_extensions[0]);
  EXPECT_EQ(L"RealAudio document", type.description);
}

// Test parsing a multi-entry description: QuickTime as provided by Totem.
TEST(MIMEDescriptionParse, Multi) {
  std::vector<WebPluginMimeType> types;
  ASSERT_TRUE(NPAPI::PluginLib::ParseMIMEDescription(
                  "video/quicktime:mov:QuickTime video;video/mp4:mp4:MPEG-4 "
                  "video;image/x-macpaint:pntg:MacPaint Bitmap image;image/x"
                  "-quicktime:pict, pict1, pict2:QuickTime image;video/x-m4v"
                  ":m4v:MPEG-4 video;",
                  &types));

  ASSERT_EQ(5U, types.size());

  // Check the x-quicktime one, since it looks tricky with spaces in the
  // extension list.
  const WebPluginMimeType& type = types[3];
  EXPECT_EQ("image/x-quicktime", type.mime_type);
  ASSERT_EQ(3U, type.file_extensions.size());
  EXPECT_EQ("pict2", type.file_extensions[2]);
  EXPECT_EQ(L"QuickTime image", type.description);
}

// Test parsing a Japanese description, since we got this wrong in the past.
// This comes from loading Totem with LANG=ja_JP.UTF-8.
TEST(MIMEDescriptionParse, JapaneseUTF8) {
  std::vector<WebPluginMimeType> types;
  ASSERT_TRUE(NPAPI::PluginLib::ParseMIMEDescription(
                  "audio/x-ogg:ogg:Ogg \xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x87"
                  "\xe3\x82\xa3\xe3\x83\xaa",
                  &types));

  ASSERT_EQ(1U, types.size());
  // Check we got the right number of Unicode characters out of the parse.
  EXPECT_EQ(9U, types[0].description.size());
}

/*

TODO(evanm): write a test that covers the following, which does *not*
parse properly with the current code.

application/x-java-vm:class,jar:IcedTea;application/x-java-applet:class,jar:IcedTea;application/x-java-applet;version=1.1:class,jar:IcedTea;application/x-java-applet;version=1.1.1:class,jar:IcedTea;application/x-java-applet;version=1.1.2:class,jar:IcedTea;application/x-java-applet;version=1.1.3:class,jar:IcedTea;application/x-java-applet;version=1.2:class,jar:IcedTea;application/x-java-applet;version=1.2.1:class,jar:IcedTea;application/x-java-applet;version=1.2.2:class,jar:IcedTea;application/x-java-applet;version=1.3:class,jar:IcedTea;application/x-java-applet;version=1.3.1:class,jar:IcedTea;application/x-java-applet;version=1.4:class,jar:IcedTea
 */


#endif  // defined(OS_LINUX)

