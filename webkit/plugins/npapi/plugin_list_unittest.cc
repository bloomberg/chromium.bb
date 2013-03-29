// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
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

}  // namespace npapi
}  // namespace webkit
