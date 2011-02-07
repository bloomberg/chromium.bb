// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "webkit/plugins/npapi/webplugininfo.h"

class DictionaryValue;
class FilePath;
class TableModelArrayControllerTest;
class PluginExceptionsTableModelTest;
class Version;

namespace webkit {
namespace npapi {

class PluginList;
namespace plugin_test_internal {
class PluginListWithoutFileIO;
}

// Hard-coded version ranges for plugin groups.
struct VersionRangeDefinition {
  // Matcher for lowest version matched by this range (inclusive). May be empty
  // to match everything iff |version_matcher_high| is also empty.
  const char* version_matcher_low;
  // Matcher for highest version matched by this range (exclusive). May be empty
  // to match anything higher than |version_matcher_low|.
  const char* version_matcher_high;
  const char* min_version;  // Minimum secure version.
  bool requires_authorization;  // If this range needs user permission to run.
};

// Hard-coded definitions of plugin groups.
struct PluginGroupDefinition {
  const char* identifier;  // Unique identifier for this group.
  const char* name;  // Name of this group.
  const char* name_matcher;  // Substring matcher for the plugin name.
  const VersionRangeDefinition* versions;  // List of version ranges.
  size_t num_versions;  // Size of the array |versions| points to.
  const char* update_url;  // Location of latest secure version.
};

// Run-time structure to hold version range information.
struct VersionRange {
 public:
  explicit VersionRange(VersionRangeDefinition definition);
  VersionRange(const VersionRange& other);
  VersionRange& operator=(const VersionRange& other);
  ~VersionRange();

  std::string low_str;
  std::string high_str;
  std::string min_str;
  scoped_ptr<Version> low;
  scoped_ptr<Version> high;
  scoped_ptr<Version> min;
  bool requires_authorization;
 private:
  void InitFrom(const VersionRange& other);
};

// A PluginGroup can match a range of versions of a specific plugin (as defined
// by matching a substring of its name).
// It contains all WebPluginInfo structs (at least one) matching its definition.
// In addition, it knows about a security "baseline", i.e. the minimum version
// of a plugin that is needed in order not to exhibit known security
// vulnerabilities.

class PluginGroup {
 public:
  // Used by about:plugins to disable Reader plugin when internal PDF viewer is
  // enabled.
  static const char* kAdobeReaderGroupName;
  static const char* kAdobeReaderUpdateURL;
  static const char* kJavaGroupName;
  static const char* kQuickTimeGroupName;
  static const char* kShockwaveGroupName;

  PluginGroup(const PluginGroup& other);

  ~PluginGroup();

  PluginGroup& operator=(const PluginGroup& other);

  // Configures the set of plugin name patterns for disabling plugins via
  // enterprise configuration management.
  static void SetPolicyDisabledPluginPatterns(const std::set<string16>& set);

  // Tests to see if a plugin is on the blacklist using its name as
  // the lookup key.
  static bool IsPluginNameDisabledByPolicy(const string16& plugin_name);

  // Returns true if the given plugin matches this group.
  bool Match(const WebPluginInfo& plugin) const;

  // Adds the given plugin to this group.
  void AddPlugin(const WebPluginInfo& plugin);

  // Removes a plugin from the group by its path.
  bool RemovePlugin(const FilePath& filename);

  // The two following functions enable/disable a plugin given its filename. The
  // function returns true if the plugin could be enabled/disabled. Plugins
  // might not get enabled/disabled if they are controlled by policy or are
  // already in the wanted state.
  bool EnablePlugin(const FilePath& filename);
  bool DisablePlugin(const FilePath& filename);

  // Enables/disables this group. This enables/disables all plugins in the
  // group.
  bool EnableGroup(bool enable);

  // Checks whether the group should be disabled/enabled by a policy and puts
  // it in the needed state. Updates all contained plugins too.
  void EnforceGroupPolicy();

  // Returns whether the plugin group is enabled or not.
  bool Enabled() const { return enabled_; }

  // Returns a unique identifier for this group, if one is defined, or the empty
  // string otherwise.
  const std::string& identifier() const { return identifier_; }

  // Sets a unique identifier for this group or if none is set an empty string.
  void set_identifier(const std::string& identifier) {
    identifier_ = identifier;
  }

  // Returns this group's name, or the filename without extension if the name
  // is empty.
  string16 GetGroupName() const;

  // Returns all plugins added to the group.
  const std::vector<WebPluginInfo>& web_plugins_info() const {
    return web_plugin_infos_;
  }

  // Checks whether a plugin exists in the group with the given path.
  bool ContainsPlugin(const FilePath& path) const;

  // Returns the description of the highest-priority plug-in in the group.
  const string16& description() const { return description_; }

  // Returns a DictionaryValue with data to display in the UI.
  DictionaryValue* GetDataForUI() const;

  // Returns a DictionaryValue with data to save in the preferences.
  DictionaryValue* GetSummary() const;

  // Returns the update URL.
  std::string GetUpdateURL() const { return update_url_; }

  // Returns true if the highest-priority plugin in this group has known
  // security problems.
  bool IsVulnerable() const;

  // Returns true if this plug-in group always requires user authorization
  // to run.
  bool RequiresAuthorization() const;

  // Check if the group has no plugins. Could happen after a reload if the plug-
  // in has disappeared from the pc (or in the process of updating).
  bool IsEmpty() const;

  // Disables all plugins in this group that are older than the
  // minimum version.
  void DisableOutdatedPlugins();

  // Parse a version string as used by a plug-in. This method is more lenient
  // in accepting weird version strings than Version::GetFromString().
  static Version* CreateVersionFromString(const string16& version_string);

  std::vector<WebPluginInfo> web_plugin_infos() { return web_plugin_infos_; }

 private:
  friend class PluginList;
  friend class plugin_test_internal::PluginListWithoutFileIO;
  friend class PluginGroupTest;
  friend class ::TableModelArrayControllerTest;
  friend class ::PluginExceptionsTableModelTest;
  FRIEND_TEST_ALL_PREFIXES(PluginListTest, DisableOutdated);

  // Generates the (short) identifier string for the given plugin.
  static std::string GetIdentifier(const WebPluginInfo& wpi);

  // Generates the long identifier (based on the full file path) for the given
  // plugin, to be called when the short identifier is not unique.
  static std::string GetLongIdentifier(const WebPluginInfo& wpi);

  // Creates a PluginGroup from a PluginGroupDefinition. The caller takes
  // ownership of the created PluginGroup.
  static PluginGroup* FromPluginGroupDefinition(
      const PluginGroupDefinition& definition);

  // Creates a PluginGroup from a WebPluginInfo. The caller takes ownership of
  // the created PluginGroup.
  static PluginGroup* FromWebPluginInfo(const WebPluginInfo& wpi);

  // Returns |true| if |version| is contained in [low, high) of |range|.
  static bool IsVersionInRange(const Version& version,
                               const VersionRange& range);

  // Returns |true| iff |plugin_version| is both contained in |version_range|
  // and declared outdated (== vulnerable) by it.
  static bool IsPluginOutdated(const Version& plugin_version,
                               const VersionRange& version_range);

  PluginGroup(const string16& group_name,
              const string16& name_matcher,
              const std::string& update_url,
              const std::string& identifier);

  void InitFrom(const PluginGroup& other);

  // Set the description and version for this plugin group from the
  // given plug-in.
  void UpdateDescriptionAndVersion(const WebPluginInfo& plugin);

  // Updates the active plugin in the group. The active plugin is the first
  // enabled one, or if all plugins are disabled, simply the first one.
  void UpdateActivePlugin(const WebPluginInfo& plugin);

  // Refreshes the enabled flag based on the state of its plugins.
  void RefreshEnabledState();

  // Enables the plugin if not already enabled and if policy allows it to.
  // Returns true on success.
  static bool Enable(WebPluginInfo* plugin, int reason);

  // Disables the plugin if not already disabled and if policy allows it to.
  // Returns true on success.
  static bool Disable(WebPluginInfo* plugin, int reason);

  // Returns a non-const vector of all plugins in the group. This is only used
  // by PluginList.
  std::vector<WebPluginInfo>& GetPluginsContainer() {
    return web_plugin_infos_;
  }

  static std::set<string16>* policy_disabled_plugin_patterns_;

  std::string identifier_;
  string16 group_name_;
  string16 name_matcher_;
  string16 description_;
  std::string update_url_;
  bool enabled_;
  std::vector<VersionRange> version_ranges_;
  scoped_ptr<Version> version_;
  std::vector<WebPluginInfo> web_plugin_infos_;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
