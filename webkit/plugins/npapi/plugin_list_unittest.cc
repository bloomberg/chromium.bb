// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
const char* kFooIdentifier = "foo";
const char* kFooGroupName = "Foo";
const char* kFooName = "Foo Plugin";
const PluginGroupDefinition kPluginDefinitions[] = {
  { kFooIdentifier, kFooGroupName, kFooName, NULL, 0,
    "http://example.com/foo" },
};

}  // namespace

class PluginListTest : public testing::Test {
 public:
  PluginListTest()
      : plugin_list_(kPluginDefinitions, arraysize(kPluginDefinitions)),
        foo_plugin_(ASCIIToUTF16(kFooName),
                    FilePath(kFooPath),
                    ASCIIToUTF16("1.2.3"),
                    ASCIIToUTF16("foo")),
        bar_plugin_(ASCIIToUTF16("Bar Plugin"),
                    FilePath(kBarPath),
                    ASCIIToUTF16("2.3.4"),
                    ASCIIToUTF16("bar")) {
  }

  virtual void SetUp() {
    plugin_list_.AddPluginToLoad(bar_plugin_);
    plugin_list_.AddPluginToLoad(foo_plugin_);
  }

  PluginGroup* AddToPluginGroups(const WebPluginInfo& plugin) {
    return plugin_list_.AddToPluginGroups(plugin, &plugin_list_.plugin_groups_);
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

TEST_F(PluginListTest, GetPluginGroup) {
  PluginGroup* foo_group = AddToPluginGroups(foo_plugin_);
  EXPECT_EQ(ASCIIToUTF16(kFooGroupName), foo_group->GetGroupName());

  scoped_ptr<PluginGroup> foo_group_copy(
      plugin_list_.GetPluginGroup(foo_plugin_));
  EXPECT_EQ(ASCIIToUTF16(kFooGroupName), foo_group_copy->GetGroupName());
}

TEST_F(PluginListTest, EmptyGroup) {
  std::vector<PluginGroup> groups;
  plugin_list_.GetPluginGroups(false, &groups);
  for (size_t i = 0; i < groups.size(); ++i)
    EXPECT_GE(1U, groups[i].web_plugin_infos().size());
}

TEST_F(PluginListTest, BadPluginDescription) {
  WebPluginInfo plugin_3043(
      string16(), FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
      string16(), string16());
  // Simulate loading of the plugins.
  plugin_list_.ClearPluginsToLoad();
  plugin_list_.AddPluginToLoad(plugin_3043);
  // Now we should have them in the state we specified above.
  plugin_list_.RefreshPlugins();
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(&plugins);
  ASSERT_TRUE(Contains(plugins, plugin_3043));
}

TEST_F(PluginListTest, HardcodedGroups) {
  std::vector<PluginGroup> groups;
  plugin_list_.GetPluginGroups(true, &groups);
  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ(1u, groups[0].web_plugin_infos().size());
  EXPECT_TRUE(groups[0].ContainsPlugin(FilePath(kBarPath)));
  EXPECT_EQ("bar.plugin", groups[0].identifier());
  EXPECT_EQ(1u, groups[1].web_plugin_infos().size());
  EXPECT_TRUE(groups[1].ContainsPlugin(FilePath(kFooPath)));
  EXPECT_EQ(kFooIdentifier, groups[1].identifier());
}

}  // namespace npapi
}  // namespace webkit
