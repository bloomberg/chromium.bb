// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

namespace webkit {
namespace npapi {

namespace {

bool Equals(const WebPluginInfo& a, const WebPluginInfo& b) {
  return (a.name == b.name &&
          a.path == b.path &&
          a.version == b.version &&
          a.desc == b.desc);
}

bool Contains(const std::vector<WebPluginInfo>& list,
              const WebPluginInfo& plugin) {
  for (std::vector<WebPluginInfo>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (Equals(*it, plugin))
      return true;
  }
  return false;
}

base::FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
base::FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
const char* kFooName = "Foo Plugin";

}  // namespace

class PluginListTest : public testing::Test {
 public:
  PluginListTest()
      : foo_plugin_(ASCIIToUTF16(kFooName),
                    base::FilePath(kFooPath),
                    ASCIIToUTF16("1.2.3"),
                    ASCIIToUTF16("foo")),
        bar_plugin_(ASCIIToUTF16("Bar Plugin"),
                    base::FilePath(kBarPath),
                    ASCIIToUTF16("2.3.4"),
                    ASCIIToUTF16("bar")) {
  }

  virtual void SetUp() {
    plugin_list_.AddPluginToLoad(bar_plugin_);
    plugin_list_.AddPluginToLoad(foo_plugin_);
  }

 protected:
  MockPluginList plugin_list_;
  WebPluginInfo foo_plugin_;
  WebPluginInfo bar_plugin_;
};

TEST_F(PluginListTest, GetPlugins) {
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(&plugins);
  EXPECT_EQ(2u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  EXPECT_TRUE(Contains(plugins, bar_plugin_));
}

TEST_F(PluginListTest, BadPluginDescription) {
  WebPluginInfo plugin_3043(
      base::string16(), base::FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
      base::string16(), base::string16());
  // Simulate loading of the plugins.
  plugin_list_.ClearPluginsToLoad();
  plugin_list_.AddPluginToLoad(plugin_3043);
  // Now we should have them in the state we specified above.
  plugin_list_.RefreshPlugins();
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(&plugins);
  ASSERT_TRUE(Contains(plugins, plugin_3043));
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)

// Test parsing a simple description: Real Audio.
TEST(MIMEDescriptionParse, Simple) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
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
  PluginList::ParseMIMEDescription(
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
  PluginList::ParseMIMEDescription(
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
  PluginList::ParseMIMEDescription("mime/type:", &types);
  EXPECT_TRUE(types.empty());

  types.clear();
  PluginList::ParseMIMEDescription("mime/type:ext1:", &types);
  ASSERT_EQ(1U, types.size());
  EXPECT_EQ("mime/type", types[0].mime_type);
  EXPECT_EQ(1U, types[0].file_extensions.size());
  EXPECT_EQ("ext1", types[0].file_extensions[0]);
  EXPECT_EQ(base::string16(), types[0].description);
}

// This Java plugin has embedded semicolons in the mime type.
TEST(MIMEDescriptionParse, ComplicatedJava) {
  std::vector<WebPluginMimeType> types;
  PluginList::ParseMIMEDescription(
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

// Make sure we understand how to get the version numbers for common Linux
// plug-ins.
TEST(PluginDescriptionParse, ExtractVersion) {
  WebPluginInfo info;
  PluginList::ExtractVersionString("Shockwave Flash 10.1 r102", &info);
  EXPECT_EQ(ASCIIToUTF16("10.1 r102"), info.version);
  PluginList::ExtractVersionString("Java(TM) Plug-in 1.6.0_22", &info);
  EXPECT_EQ(ASCIIToUTF16("1.6.0_22"), info.version);
  // It's actually much more likely for a modern Linux distribution to have
  // IcedTea.
  PluginList::ExtractVersionString(
      "IcedTea-Web Plugin "
      "(using IcedTea-Web 1.2 (1.2-2ubuntu0.10.04.2))",
      &info);
  EXPECT_EQ(ASCIIToUTF16("1.2"), info.version);
}

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)


}  // namespace npapi
}  // namespace webkit
