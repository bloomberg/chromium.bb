// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_group.h"

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/webplugininfo.h"

namespace webkit {
namespace npapi {

static const VersionRangeDefinition kPluginVersionRange[] = {
    { "", "", "3.0.44", false }
};
static const VersionRangeDefinition kPlugin3VersionRange[] = {
    { "0", "4", "3.0.44", false }
};
static const VersionRangeDefinition kPlugin4VersionRange[] = {
    { "4", "5", "4.0.44", false }
};
static const VersionRangeDefinition kPlugin34VersionRange[] = {
    { "0", "4", "3.0.44", false },
    { "4", "5", "", false }
};

static const PluginGroupDefinition kPluginDef = {
    "myplugin", "MyPlugin", "MyPlugin", kPluginVersionRange,
    arraysize(kPluginVersionRange), "http://latest/" };
static const PluginGroupDefinition kPluginDef3 = {
    "myplugin-3", "MyPlugin 3", "MyPlugin", kPlugin3VersionRange,
    arraysize(kPlugin3VersionRange), "http://latest" };
static const PluginGroupDefinition kPluginDef4 = {
    "myplugin-4", "MyPlugin 4", "MyPlugin", kPlugin4VersionRange,
    arraysize(kPlugin4VersionRange), "http://latest" };
static const PluginGroupDefinition kPluginDef34 = {
    "myplugin-34", "MyPlugin 3/4", "MyPlugin", kPlugin34VersionRange,
    arraysize(kPlugin34VersionRange), "http://latest" };
static const PluginGroupDefinition kPluginDefNotVulnerable = {
    "myplugin-latest", "MyPlugin", "MyPlugin", NULL, 0, "http://latest" };

const PluginGroupDefinition kPluginDefinitions[] = {
  kPluginDef,
  kPluginDef3,
  kPluginDef4,
  kPluginDef34,
  kPluginDefNotVulnerable,
};

// name, path, version, desc.
static const WebPluginInfo kPluginNoVersion = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.2.0.43")),
    ASCIIToUTF16(""), ASCIIToUTF16("MyPlugin version 2.0.43"));
static const WebPluginInfo kPlugin2043 = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.2.0.43")),
    ASCIIToUTF16("2.0.43"), ASCIIToUTF16("MyPlugin version 2.0.43"));
static const WebPluginInfo kPlugin3043 = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.3.0.43")),
    ASCIIToUTF16("3.0.43"), ASCIIToUTF16("MyPlugin version 3.0.43"));
static const WebPluginInfo kPlugin3044 = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.3.0.44")),
    ASCIIToUTF16("3.0.44"), ASCIIToUTF16("MyPlugin version 3.0.44"));
static const WebPluginInfo kPlugin3045 = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.3.0.45")),
     ASCIIToUTF16("3.0.45"), ASCIIToUTF16("MyPlugin version 3.0.45"));
static const WebPluginInfo kPlugin3045r = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.3.0.45")),
     ASCIIToUTF16("3.0r45"), ASCIIToUTF16("MyPlugin version 3.0r45"));
static const WebPluginInfo kPlugin4043 = WebPluginInfo(
    ASCIIToUTF16("MyPlugin"), FilePath(FILE_PATH_LITERAL("myplugin.so.4.0.43")),
     ASCIIToUTF16("4.0.43"), ASCIIToUTF16("MyPlugin version 4.0.43"));

class PluginGroupTest : public testing::Test {
 public:
  static PluginGroup* CreatePluginGroup(
      const PluginGroupDefinition& definition) {
    return PluginGroup::FromPluginGroupDefinition(definition);
  }
  static PluginGroup* CreatePluginGroup(const WebPluginInfo& wpi) {
    return PluginGroup::FromWebPluginInfo(wpi);
  }
 protected:
  virtual void TearDown() {
    PluginGroup::SetPolicyEnforcedPluginPatterns(std::set<string16>(),
                                                 std::set<string16>(),
                                                 std::set<string16>());
  }
};

TEST_F(PluginGroupTest, PluginGroupMatch) {
  scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
      kPluginDef3));
  EXPECT_TRUE(group->Match(kPlugin3045));
  EXPECT_TRUE(group->Match(kPlugin3045r));
  EXPECT_FALSE(group->Match(kPluginNoVersion));
  group->AddPlugin(kPlugin3045);
  EXPECT_FALSE(group->IsVulnerable());

  group.reset(PluginGroupTest::CreatePluginGroup(kPluginDef));
  EXPECT_FALSE(group->Match(kPluginNoVersion));
}

TEST_F(PluginGroupTest, PluginGroupMatchCorrectVersion) {
  scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
      kPluginDef3));
  EXPECT_TRUE(group->Match(kPlugin2043));
  EXPECT_TRUE(group->Match(kPlugin3043));
  EXPECT_FALSE(group->Match(kPlugin4043));

  group.reset(PluginGroupTest::CreatePluginGroup(kPluginDef4));
  EXPECT_FALSE(group->Match(kPlugin2043));
  EXPECT_FALSE(group->Match(kPlugin3043));
  EXPECT_TRUE(group->Match(kPlugin4043));

  group.reset(PluginGroupTest::CreatePluginGroup(kPluginDef34));
  EXPECT_TRUE(group->Match(kPlugin2043));
  EXPECT_TRUE(group->Match(kPlugin3043));
  EXPECT_TRUE(group->Match(kPlugin4043));
}

TEST_F(PluginGroupTest, PluginGroupDescription) {
  string16 desc3043(ASCIIToUTF16("MyPlugin version 3.0.43"));
  string16 desc3045(ASCIIToUTF16("MyPlugin version 3.0.45"));

  PluginGroupDefinition plugindefs[] =
      { kPluginDef, kPluginDef3, kPluginDef34 };
  for (size_t i = 0; i < arraysize(plugindefs); ++i) {
    WebPluginInfo plugin3043(kPlugin3043);
    WebPluginInfo plugin3045(kPlugin3045);
    {
      scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
          plugindefs[i]));
      EXPECT_TRUE(group->Match(plugin3043));
      group->AddPlugin(plugin3043);
      EXPECT_EQ(desc3043, group->description());
      EXPECT_TRUE(group->IsVulnerable());
      EXPECT_TRUE(group->Match(plugin3045));
      group->AddPlugin(plugin3045);
      EXPECT_EQ(desc3043, group->description());
      EXPECT_TRUE(group->IsVulnerable());
    }
    {
      // Disable the second plugin.
      plugin3045.enabled =
          webkit::npapi::WebPluginInfo::USER_DISABLED_POLICY_UNMANAGED;
      scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
          plugindefs[i]));
      EXPECT_TRUE(group->Match(plugin3043));
      group->AddPlugin(plugin3043);
      EXPECT_EQ(desc3043, group->description());
      EXPECT_TRUE(group->IsVulnerable());
      EXPECT_TRUE(group->Match(plugin3045));
      group->AddPlugin(plugin3045);
      EXPECT_EQ(desc3043, group->description());
      EXPECT_TRUE(group->IsVulnerable());
    }
  }
}

TEST_F(PluginGroupTest, PluginGroupDefinition) {
  for (size_t i = 0; i < arraysize(kPluginDefinitions); ++i) {
    scoped_ptr<PluginGroup> def_group(
        PluginGroupTest::CreatePluginGroup(kPluginDefinitions[i]));
    ASSERT_TRUE(def_group.get() != NULL);
  }
}

TEST_F(PluginGroupTest, DisableOutdated) {
  PluginGroupDefinition plugindefs[] = { kPluginDef3, kPluginDef34 };
  for (size_t i = 0; i < 2; ++i) {
    scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
        plugindefs[i]));
    group->AddPlugin(kPlugin3043);
    group->AddPlugin(kPlugin3045);

    EXPECT_EQ(ASCIIToUTF16("MyPlugin version 3.0.43"), group->description());
    EXPECT_TRUE(group->IsVulnerable());

    group->DisableOutdatedPlugins();
    EXPECT_EQ(ASCIIToUTF16("MyPlugin version 3.0.45"), group->description());
    EXPECT_FALSE(group->IsVulnerable());
  }
}

TEST_F(PluginGroupTest, VersionExtraction) {
  // Some real-world plugin versions (spaces, commata, parentheses, 'r', oh my)
  const char* versions[][2] = {
    { "7.6.6 (1671)", "7.6.6.1671" },  // Quicktime
    { "2, 0, 0, 254", "2.0.0.254" },   // DivX
    { "3, 0, 0, 0", "3.0.0.0" },       // Picasa
    { "1, 0, 0, 1", "1.0.0.1" },       // Earth
    { "10,0,45,2", "10.0.45.2" },      // Flash
    { "10.1 r102", "10.1.102"},        // Flash
    { "10.3 d180", "10.3.180" },       // Flash (Debug)
    { "11.5.7r609", "11.5.7.609"},     // Shockwave
    { "1.6.0_22", "1.6.0.22"},         // Java
  };

  for (size_t i = 0; i < arraysize(versions); i++) {
    const WebPluginInfo plugin = WebPluginInfo(
        ASCIIToUTF16("Blah Plugin"), FilePath(FILE_PATH_LITERAL("blahfile")),
        ASCIIToUTF16(versions[i][0]), string16());
    scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(plugin));
    EXPECT_TRUE(group->Match(plugin));
    group->AddPlugin(plugin);
    scoped_ptr<DictionaryValue> data(group->GetDataForUI());
    std::string version;
    data->GetString("version", &version);
    EXPECT_EQ(versions[i][1], version);
  }
}

TEST_F(PluginGroupTest, DisabledByPolicy) {
  std::set<string16> disabled_plugins;
  disabled_plugins.insert(ASCIIToUTF16("Disable this!"));
  disabled_plugins.insert(ASCIIToUTF16("*Google*"));
  PluginGroup::SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                               std::set<string16>(),
                                               std::set<string16>());

  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(ASCIIToUTF16("42")));
  EXPECT_TRUE(PluginGroup::IsPluginNameDisabledByPolicy(
      ASCIIToUTF16("Disable this!")));
  EXPECT_TRUE(PluginGroup::IsPluginNameDisabledByPolicy(
      ASCIIToUTF16("Google Earth")));
}

TEST_F(PluginGroupTest, EnabledByPolicy) {
  std::set<string16> enabled_plugins;
  enabled_plugins.insert(ASCIIToUTF16("Enable that!"));
  enabled_plugins.insert(ASCIIToUTF16("PDF*"));
  PluginGroup::SetPolicyEnforcedPluginPatterns(std::set<string16>(),
                                               std::set<string16>(),
                                               enabled_plugins);

  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(ASCIIToUTF16("42")));
  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(
      ASCIIToUTF16("Enable that!")));
  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(
      ASCIIToUTF16("PDF Reader")));
}

TEST_F(PluginGroupTest, EnabledAndDisabledByPolicy) {
  const string16 k42(ASCIIToUTF16("42"));
  const string16 kEnabled(ASCIIToUTF16("Enabled"));
  const string16 kEnabled2(ASCIIToUTF16("Enabled 2"));
  const string16 kEnabled3(ASCIIToUTF16("Enabled 3"));
  const string16 kException(ASCIIToUTF16("Exception"));
  const string16 kException2(ASCIIToUTF16("Exception 2"));
  const string16 kGoogleMars(ASCIIToUTF16("Google Mars"));
  const string16 kGoogleEarth(ASCIIToUTF16("Google Earth"));

  std::set<string16> disabled_plugins;
  std::set<string16> disabled_plugins_exceptions;
  std::set<string16> enabled_plugins;

  disabled_plugins.insert(kEnabled);
  disabled_plugins_exceptions.insert(kEnabled);
  enabled_plugins.insert(kEnabled);

  disabled_plugins_exceptions.insert(kException);

  disabled_plugins.insert(kEnabled2);
  enabled_plugins.insert(kEnabled2);

  disabled_plugins.insert(kException2);
  disabled_plugins_exceptions.insert(kException2);

  disabled_plugins_exceptions.insert(kEnabled3);
  enabled_plugins.insert(kEnabled3);

  PluginGroup::SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                               disabled_plugins_exceptions,
                                               enabled_plugins);

  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(k42));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(k42));

  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(kEnabled));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kEnabled));
  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(kEnabled2));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kEnabled2));
  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(kEnabled3));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kEnabled3));

  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(kException));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kException));
  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(kException2));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kException2));

  disabled_plugins.clear();
  disabled_plugins_exceptions.clear();
  enabled_plugins.clear();

  disabled_plugins.insert(ASCIIToUTF16("*"));
  disabled_plugins_exceptions.insert(ASCIIToUTF16("*Google*"));
  enabled_plugins.insert(kGoogleEarth);

  PluginGroup::SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                               disabled_plugins_exceptions,
                                               enabled_plugins);

  EXPECT_TRUE(PluginGroup::IsPluginNameEnabledByPolicy(kGoogleEarth));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kGoogleEarth));
  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(kGoogleMars));
  EXPECT_FALSE(PluginGroup::IsPluginNameDisabledByPolicy(kGoogleMars));
  EXPECT_FALSE(PluginGroup::IsPluginNameEnabledByPolicy(k42));
  EXPECT_TRUE(PluginGroup::IsPluginNameDisabledByPolicy(k42));
}

TEST_F(PluginGroupTest, IsVulnerable) {
  // Adobe Reader 10
  VersionRangeDefinition adobe_reader_version_range[] = {
      { "10", "11", "", false },
      { "9", "10", "9.4.1", false },
      { "0", "9", "8.2.5", false }
  };
  PluginGroupDefinition adobe_reader_plugin_def = {
      "adobe-reader", "Adobe Reader", "Adobe Acrobat",
      adobe_reader_version_range, arraysize(adobe_reader_version_range),
      "http://get.adobe.com/reader/" };
  WebPluginInfo adobe_reader_plugin(ASCIIToUTF16("Adobe Reader"),
                                    FilePath(FILE_PATH_LITERAL("/reader.so")),
                                    ASCIIToUTF16("10.0.0.396"),
                                    ASCIIToUTF16("adobe reader 10"));
  scoped_ptr<PluginGroup> group(PluginGroupTest::CreatePluginGroup(
      adobe_reader_plugin_def));
  group->AddPlugin(adobe_reader_plugin);
  PluginGroup group_copy(*group);  // Exercise the copy constructor.
  EXPECT_FALSE(group_copy.IsVulnerable());
  EXPECT_FALSE(group_copy.RequiresAuthorization());

  // Silverlight 4
  VersionRangeDefinition silverlight_version_range[] = {
      { "0", "4", "3.0.50106.0", false },
      { "4", "5", "", true }
  };
  PluginGroupDefinition silverlight_plugin_def = {
      "silverlight", "Silverlight", "Silverlight", silverlight_version_range,
      arraysize(silverlight_version_range),
      "http://www.microsoft.com/getsilverlight/" };
  WebPluginInfo silverlight_plugin(ASCIIToUTF16("Silverlight"),
                                   FilePath(FILE_PATH_LITERAL("/silver.so")),
                                   ASCIIToUTF16("4.0.50917.0"),
                                   ASCIIToUTF16("silverlight 4"));
  group.reset(PluginGroupTest::CreatePluginGroup(silverlight_plugin_def));
  group->AddPlugin(silverlight_plugin);
  EXPECT_FALSE(PluginGroup(*group).IsVulnerable());
  EXPECT_TRUE(PluginGroup(*group).RequiresAuthorization());
}

TEST_F(PluginGroupTest, MultipleVersions) {
  scoped_ptr<PluginGroup> group(
      PluginGroupTest::CreatePluginGroup(kPluginDef3));
  group->AddPlugin(kPlugin3044);
  group->AddPlugin(kPlugin3043);
  EXPECT_EQ(kPlugin3044.desc, group->description());
  EXPECT_FALSE(group->IsVulnerable());

  group->DisablePlugin(kPlugin3044.path);
  EXPECT_EQ(kPlugin3043.desc, group->description());
  EXPECT_TRUE(group->IsVulnerable());

  EXPECT_TRUE(group->EnableGroup(false));
  EXPECT_EQ(kPlugin3044.desc, group->description());
  EXPECT_FALSE(group->IsVulnerable());

  EXPECT_TRUE(group->RemovePlugin(kPlugin3044.path));
  EXPECT_EQ(kPlugin3043.desc, group->description());
  EXPECT_TRUE(group->IsVulnerable());

  EXPECT_TRUE(group->RemovePlugin(kPlugin3043.path));
  EXPECT_TRUE(group->IsEmpty());
  EXPECT_EQ(string16(), group->description());
}

}  // namespace npapi
}  // namespace webkit
