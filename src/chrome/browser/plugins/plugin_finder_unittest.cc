// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include "base/values.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::DictionaryValue;
using ::base::ListValue;
using ::testing::Optional;

TEST(PluginFinderTest, JsonSyntax) {
  std::unique_ptr<base::DictionaryValue> plugin_list =
      PluginFinder::LoadBuiltInPluginList();
  ASSERT_TRUE(plugin_list);
  absl::optional<base::Value> version = plugin_list->ExtractKey("x-version");
  ASSERT_TRUE(version.has_value());
  EXPECT_EQ(base::Value::Type::INTEGER, version->type());

  for (base::DictionaryValue::Iterator plugin_it(*plugin_list);
       !plugin_it.IsAtEnd(); plugin_it.Advance()) {
    const base::DictionaryValue* plugin = NULL;
    ASSERT_TRUE(plugin_it.value().GetAsDictionary(&plugin));
    std::string dummy_str;
    if (plugin->HasKey("lang"))
      EXPECT_TRUE(plugin->GetString("lang", &dummy_str));
    if (plugin->HasKey("url"))
      EXPECT_TRUE(plugin->GetString("url", &dummy_str));
    EXPECT_TRUE(plugin->GetString("name", &dummy_str));
    if (plugin->HasKey("help_url"))
      EXPECT_TRUE(plugin->GetString("help_url", &dummy_str));
    if (plugin->HasKey("displayurl")) {
      EXPECT_THAT(plugin->FindBoolKey("displayurl"), Optional(true));
    }
    if (plugin->HasKey("requires_authorization"))
      EXPECT_TRUE(plugin->FindBoolKey("requires_authorization").has_value());
    const base::ListValue* mime_types = NULL;
    if (plugin->GetList("mime_types", &mime_types)) {
      for (const auto& mime_type : mime_types->GetList()) {
        EXPECT_TRUE(mime_type.GetAsString(&dummy_str));
      }
    }

    const base::ListValue* matching_mime_types = NULL;
    if (plugin->GetList("matching_mime_types", &matching_mime_types)) {
      for (const auto& mime_type : matching_mime_types->GetList()) {
        EXPECT_TRUE(mime_type.GetAsString(&dummy_str));
      }
    }

    const base::ListValue* versions = NULL;
    if (!plugin->GetList("versions", &versions))
      continue;

    for (const auto& version_value : versions->GetList()) {
      const base::DictionaryValue* version_dict = nullptr;
      ASSERT_TRUE(version_value.GetAsDictionary(&version_dict));
      EXPECT_TRUE(version_dict->GetString("version", &dummy_str));
      std::string status_str;
      EXPECT_TRUE(version_dict->GetString("status", &status_str));
      PluginMetadata::SecurityStatus status =
          PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
      EXPECT_TRUE(PluginMetadata::ParseSecurityStatus(status_str, &status))
          << "Invalid security status \"" << status_str << "\"";
    }
  }
}

TEST(PluginFinderTest, ReinitializePlugins) {
  PluginFinder* plugin_finder = PluginFinder::GetInstance();

  plugin_finder->Init();

  std::unique_ptr<base::DictionaryValue> plugin_list =
      PluginFinder::LoadBuiltInPluginList();

  // Increment the version number by one.
  const base::Value* version_value = plugin_list->FindKey("x-version");
  ASSERT_TRUE(version_value);
  ASSERT_TRUE(version_value->is_int());
  plugin_list->SetKey("x-version",
                      base::Value(version_value->GetIfInt().value_or(0) + 1));

  plugin_finder->ReinitializePlugins(plugin_list.get());
}
