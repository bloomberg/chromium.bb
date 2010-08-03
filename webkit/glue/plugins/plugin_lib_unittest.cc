// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_lib.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test the unloading of plugin libs. Bug http://crbug.com/46526 showed that
// if UnloadAllPlugins() simply iterates through the g_loaded_libs global
// variable, we can get a crash if no plugin libs were marked as always loaded.
class PluginLibTest : public NPAPI::PluginLib {
 public:
  PluginLibTest() : NPAPI::PluginLib(WebPluginInfo(), NULL) {
  }
  using NPAPI::PluginLib::Unload;
};

TEST(PluginLibLoading, UnloadAllPlugins) {
  // For the creation of the g_loaded_libs global variable.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(FilePath()));

  // Try with a single plugin lib.
  scoped_refptr<PluginLibTest> plugin_lib1 = new PluginLibTest();
  NPAPI::PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(FilePath()));

  // Try with two plugin libs.
  plugin_lib1 = new PluginLibTest();
  scoped_refptr<PluginLibTest> plugin_lib2 = new PluginLibTest();
  NPAPI::PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(FilePath()));

  // Now try to manually Unload one and then UnloadAll.
  plugin_lib1 = new PluginLibTest();
  plugin_lib2 = new PluginLibTest();
  plugin_lib1->Unload();
  NPAPI::PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(FilePath()));

  // Now try to manually Unload the only one and then UnloadAll.
  plugin_lib1 = new PluginLibTest();
  plugin_lib1->Unload();
  NPAPI::PluginLib::UnloadAllPlugins();
}

#if defined(OS_LINUX)

// Test parsing a simple description: Real Audio.
TEST(MIMEDescriptionParse, Simple) {
  std::vector<WebPluginMimeType> types;
  NPAPI::PluginLib::ParseMIMEDescription(
      "audio/x-pn-realaudio-plugin:rpm:RealAudio document;",
      &types);
  ASSERT_EQ(1U, types.size());
  const WebPluginMimeType& type = types[0];
  EXPECT_EQ("audio/x-pn-realaudio-plugin", type.mime_type);
  ASSERT_EQ(1U, type.file_extensions.size());
  EXPECT_EQ("rpm", type.file_extensions[0]);
  EXPECT_EQ(ASCIIToUTF16("RealAudio document"), type.description);
}

// Test parsing a multi-entry description: QuickTime as provided by Totem.
TEST(MIMEDescriptionParse, Multi) {
  std::vector<WebPluginMimeType> types;
  NPAPI::PluginLib::ParseMIMEDescription(
      "video/quicktime:mov:QuickTime video;video/mp4:mp4:MPEG-4 "
      "video;image/x-macpaint:pntg:MacPaint Bitmap image;image/x"
      "-quicktime:pict, pict1, pict2:QuickTime image;video/x-m4v"
      ":m4v:MPEG-4 video;",
      &types);

  ASSERT_EQ(5U, types.size());

  // Check the x-quicktime one, since it looks tricky with spaces in the
  // extension list.
  const WebPluginMimeType& type = types[3];
  EXPECT_EQ("image/x-quicktime", type.mime_type);
  ASSERT_EQ(3U, type.file_extensions.size());
  EXPECT_EQ("pict2", type.file_extensions[2]);
  EXPECT_EQ(ASCIIToUTF16("QuickTime image"), type.description);
}

// Test parsing a Japanese description, since we got this wrong in the past.
// This comes from loading Totem with LANG=ja_JP.UTF-8.
TEST(MIMEDescriptionParse, JapaneseUTF8) {
  std::vector<WebPluginMimeType> types;
  NPAPI::PluginLib::ParseMIMEDescription(
      "audio/x-ogg:ogg:Ogg \xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x87"
      "\xe3\x82\xa3\xe3\x83\xaa",
      &types);

  ASSERT_EQ(1U, types.size());
  // Check we got the right number of Unicode characters out of the parse.
  EXPECT_EQ(9U, types[0].description.size());
}

// Test that we handle corner cases gracefully.
TEST(MIMEDescriptionParse, CornerCases) {
  std::vector<WebPluginMimeType> types;
  NPAPI::PluginLib::ParseMIMEDescription("mime/type:", &types);
  EXPECT_TRUE(types.empty());

  types.clear();
  NPAPI::PluginLib::ParseMIMEDescription("mime/type:ext1:", &types);
  ASSERT_EQ(1U, types.size());
  EXPECT_EQ("mime/type", types[0].mime_type);
  EXPECT_EQ(1U, types[0].file_extensions.size());
  EXPECT_EQ("ext1", types[0].file_extensions[0]);
  EXPECT_EQ(string16(), types[0].description);
}

// This Java plugin has embedded semicolons in the mime type.
TEST(MIMEDescriptionParse, ComplicatedJava) {
  std::vector<WebPluginMimeType> types;
  NPAPI::PluginLib::ParseMIMEDescription(
      "application/x-java-vm:class,jar:IcedTea;application/x-java"
      "-applet:class,jar:IcedTea;application/x-java-applet;versio"
      "n=1.1:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.1:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.2:class,jar:IcedTea;application/x-java-applet;version="
      "1.1.3:class,jar:IcedTea;application/x-java-applet;version="
      "1.2:class,jar:IcedTea;application/x-java-applet;version=1."
      "2.1:class,jar:IcedTea;application/x-java-applet;version=1."
      "2.2:class,jar:IcedTea;application/x-java-applet;version=1."
      "3:class,jar:IcedTea;application/x-java-applet;version=1.3."
      "1:class,jar:IcedTea;application/x-java-applet;version=1.4:"
      "class,jar:IcedTea",
      &types);

  ASSERT_EQ(12U, types.size());
  for (size_t i = 0; i < types.size(); ++i)
    EXPECT_EQ(ASCIIToUTF16("IcedTea"), types[i].description);

  // Verify that the mime types with semis are coming through ok.
  EXPECT_TRUE(types[4].mime_type.find(';') != std::string::npos);
}

#endif  // defined(OS_LINUX)
