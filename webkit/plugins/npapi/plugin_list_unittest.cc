// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webkit {
namespace npapi {

namespace plugin_test_internal {

// A PluginList for tests that avoids file system IO. There is also no reason
// to use |lock_| (but it doesn't hurt either).
class PluginListWithoutFileIO : public PluginList {
 public:
  void AddPluginToLoad(const WebPluginInfo& plugin) {
    plugins_to_load_.push_back(plugin);
  }

  void ClearPluginsToLoad() {
    plugins_to_load_.clear();
  }

  void AddPluginGroupDefinition(const PluginGroupDefinition& definition) {
    hardcoded_definitions_.push_back(definition);
  }

 private:
  std::vector<WebPluginInfo> plugins_to_load_;
  std::vector<PluginGroupDefinition> hardcoded_definitions_;

  // PluginList methods:
  virtual void LoadPluginsInternal(
      ScopedVector<PluginGroup>* plugin_groups) OVERRIDE {
    for (size_t i = 0; i < plugins_to_load_.size(); ++i)
      AddToPluginGroups(plugins_to_load_[i], plugin_groups);
  }

  virtual const PluginGroupDefinition* GetPluginGroupDefinitions() OVERRIDE {
    return &hardcoded_definitions_.front();
  }
  virtual size_t GetPluginGroupDefinitionsSize() OVERRIDE {
    return hardcoded_definitions_.size();
  }
};

}  // namespace plugin_test_internal

namespace {

bool Equals(const WebPluginInfo& a, const WebPluginInfo& b,
            bool care_about_enabled_status) {
  return (a.name == b.name &&
          a.path == b.path &&
          a.version == b.version &&
          a.desc == b.desc &&
          (!care_about_enabled_status || a.enabled == b.enabled));
}

bool Contains(const std::vector<WebPluginInfo>& list,
              const WebPluginInfo& plugin,
              bool care_about_enabled_status) {
  for (std::vector<WebPluginInfo>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (Equals(*it, plugin, care_about_enabled_status))
      return true;
  }
  return false;
}

FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
const char* kFooIdentifier = "foo";
const char* kFooName = "Foo";


}  // namespace

class PluginListTest : public testing::Test {
 public:
  PluginListTest()
      : foo_plugin_(ASCIIToUTF16("Foo Plugin"),
                    FilePath(kFooPath),
                    ASCIIToUTF16("1.2.3"),
                    ASCIIToUTF16("foo")),
        bar_plugin_(ASCIIToUTF16("Bar Plugin"),
                    FilePath(kBarPath),
                    ASCIIToUTF16("2.3.4"),
                    ASCIIToUTF16("bar")) {
  }

  virtual void SetUp() {
    bar_plugin_.enabled = WebPluginInfo::USER_DISABLED_POLICY_UNMANAGED;
    plugin_list_.DisablePlugin(bar_plugin_.path);
    plugin_list_.AddPluginToLoad(foo_plugin_);
    plugin_list_.AddPluginToLoad(bar_plugin_);
    PluginGroupDefinition foo_definition = { kFooIdentifier, kFooName,
                                             "Foo Plugin", NULL, 0,
                                             "http://example.com/foo" };
    plugin_list_.AddPluginGroupDefinition(foo_definition);
    plugin_list_.LoadPlugins(true);
  }

 protected:
  plugin_test_internal::PluginListWithoutFileIO plugin_list_;
  WebPluginInfo foo_plugin_;
  WebPluginInfo bar_plugin_;
};

TEST_F(PluginListTest, GetPlugins) {
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(false, &plugins);
  EXPECT_EQ(2u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_, true));
  EXPECT_TRUE(Contains(plugins, bar_plugin_, true));
}

TEST_F(PluginListTest, GetEnabledPlugins) {
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetEnabledPlugins(false, &plugins);
  EXPECT_EQ(1u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_, true));
}

TEST_F(PluginListTest, GetPluginGroup) {
  const PluginGroup* foo_group = plugin_list_.GetPluginGroup(foo_plugin_);
  EXPECT_EQ(ASCIIToUTF16(kFooName), foo_group->GetGroupName());
  EXPECT_TRUE(foo_group->Enabled());
  // The second request should return a pointer to the same instance.
  const PluginGroup* foo_group2 = plugin_list_.GetPluginGroup(foo_plugin_);
  EXPECT_EQ(foo_group, foo_group2);
  const PluginGroup* bar_group = plugin_list_.GetPluginGroup(bar_plugin_);
  EXPECT_FALSE(bar_group->Enabled());
}

TEST_F(PluginListTest, EnableDisablePlugin) {
  // Disable "foo" plugin.
  plugin_list_.DisablePlugin(foo_plugin_.path);
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetEnabledPlugins(false, &plugins);
  EXPECT_FALSE(Contains(plugins, foo_plugin_, false));
  const PluginGroup* foo_group = plugin_list_.GetPluginGroup(foo_plugin_);
  EXPECT_FALSE(foo_group->Enabled());
  // Enable "bar" plugin.
  plugin_list_.EnablePlugin(bar_plugin_.path);
  plugin_list_.GetEnabledPlugins(false, &plugins);
  EXPECT_TRUE(Contains(plugins, bar_plugin_, false));
  const PluginGroup* bar_group = plugin_list_.GetPluginGroup(bar_plugin_);
  EXPECT_TRUE(bar_group->Enabled());
}

TEST_F(PluginListTest, EnableGroup) {
  // Disable "foo" plugin group.
  const PluginGroup* foo_group = plugin_list_.GetPluginGroup(foo_plugin_);
  EXPECT_TRUE(foo_group->Enabled());
  EXPECT_TRUE(plugin_list_.EnableGroup(false, foo_group->GetGroupName()));
  EXPECT_FALSE(foo_group->Enabled());
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetEnabledPlugins(false, &plugins);
  EXPECT_EQ(0u, plugins.size());
  EXPECT_FALSE(Contains(plugins, foo_plugin_, false));
  // Enable "bar" plugin group.
  const PluginGroup* bar_group = plugin_list_.GetPluginGroup(bar_plugin_);
  EXPECT_FALSE(bar_group->Enabled());
  plugin_list_.EnableGroup(true, bar_group->GetGroupName());
  EXPECT_TRUE(bar_group->Enabled());
  plugin_list_.GetEnabledPlugins(false, &plugins);
  EXPECT_TRUE(Contains(plugins, bar_plugin_, false));
}

TEST_F(PluginListTest, EmptyGroup) {
  std::vector<PluginGroup> groups;
  plugin_list_.GetPluginGroups(false, &groups);
  for (size_t i = 0; i < groups.size(); ++i)
    EXPECT_GE(1U, groups[i].web_plugins_info().size());
}

TEST_F(PluginListTest, DisableOutdated) {
  VersionRangeDefinition version_range[] = {
      { "0", "4", "3.0.44" },
      { "4", "5", "" }
  };
  WebPluginInfo plugin_3043(ASCIIToUTF16("MyPlugin"),
                            FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
                            ASCIIToUTF16("3.0.43"),
                            ASCIIToUTF16("MyPlugin version 3.0.43"));
  WebPluginInfo plugin_3045(ASCIIToUTF16("MyPlugin"),
                            FilePath(FILE_PATH_LITERAL("/myplugin.3.0.45")),
                            ASCIIToUTF16("3.0.45"),
                            ASCIIToUTF16("MyPlugin version 3.0.45"));
  plugin_list_.ClearPluginsToLoad();
  plugin_list_.AddPluginToLoad(plugin_3043);
  plugin_list_.AddPluginToLoad(plugin_3045);
  // Enfore the load to run.
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(true, &plugins);
  PluginGroup* group_3043 =
      const_cast<PluginGroup*>(plugin_list_.GetPluginGroup(plugin_3043));
  const PluginGroup* group_3045 = plugin_list_.GetPluginGroup(plugin_3045);
  EXPECT_EQ(group_3043, group_3045);
  group_3043->version_ranges_.push_back(VersionRange(version_range[0]));
  group_3043->version_ranges_.push_back(VersionRange(version_range[1]));
  EXPECT_EQ(plugin_3043.desc, group_3043->description());
  EXPECT_TRUE(group_3043->IsVulnerable());
  group_3043->DisableOutdatedPlugins();
  EXPECT_EQ(plugin_3045.desc, group_3043->description());
  EXPECT_FALSE(group_3043->IsVulnerable());
}

TEST_F(PluginListTest, BadPluginDescription) {
  WebPluginInfo plugin_3043(ASCIIToUTF16(""),
                            FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
                            ASCIIToUTF16(""),
                            ASCIIToUTF16(""));
  // Simulate loading of the plugins.
  plugin_list_.ClearPluginsToLoad();
  plugin_list_.AddPluginToLoad(plugin_3043);
  // Now we should have them in the state we specified above.
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(true, &plugins);
  ASSERT_TRUE(Contains(plugins, plugin_3043, true));
}

TEST_F(PluginListTest, DisableAndEnableBeforeLoad) {
  WebPluginInfo plugin_3043(ASCIIToUTF16("MyPlugin"),
                            FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
                            ASCIIToUTF16("3.0.43"),
                            ASCIIToUTF16("MyPlugin version 3.0.43"));
  WebPluginInfo plugin_3045(ASCIIToUTF16("MyPlugin"),
                            FilePath(FILE_PATH_LITERAL("/myplugin.3.0.45")),
                            ASCIIToUTF16("3.0.45"),
                            ASCIIToUTF16("MyPlugin version 3.0.45"));
  // Disable the first one and disable and then enable the second one.
  EXPECT_TRUE(plugin_list_.DisablePlugin(plugin_3043.path));
  EXPECT_TRUE(plugin_list_.DisablePlugin(plugin_3045.path));
  EXPECT_TRUE(plugin_list_.EnablePlugin(plugin_3045.path));
  // Simulate loading of the plugins.
  plugin_list_.ClearPluginsToLoad();
  plugin_list_.AddPluginToLoad(plugin_3043);
  plugin_list_.AddPluginToLoad(plugin_3045);
  // Now we should have them in the state we specified above.
  std::vector<WebPluginInfo> plugins;
  plugin_list_.GetPlugins(true, &plugins);
  plugin_3043.enabled = WebPluginInfo::USER_DISABLED_POLICY_UNMANAGED;
  ASSERT_TRUE(Contains(plugins, plugin_3043, true));
  ASSERT_TRUE(Contains(plugins, plugin_3045, true));
}

TEST_F(PluginListTest, HardcodedGroups) {
  std::vector<PluginGroup> groups;
  plugin_list_.GetPluginGroups(true, &groups);
  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ(1u, groups[0].web_plugins_info().size());
  EXPECT_TRUE(groups[0].ContainsPlugin(FilePath(kFooPath)));
  EXPECT_EQ(kFooIdentifier, groups[0].identifier());
  EXPECT_EQ(1u, groups[1].web_plugins_info().size());
  EXPECT_TRUE(groups[1].ContainsPlugin(FilePath(kBarPath)));
  EXPECT_EQ("bar.plugin", groups[1].identifier());
}

}  // namespace npapi
}  // namespace webkit
