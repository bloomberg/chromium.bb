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

class DictionaryValue;
class FilePath;
class TableModelArrayControllerTest;
class PluginExceptionsTableModelTest;
class Version;

namespace webkit {
namespace npapi {

class PluginList;
struct WebPluginInfo;

// Hard-coded version ranges for plugin groups.
struct VersionRangeDefinition {
  // Matcher for lowest version matched by this range (inclusive). May be empty
  // to match everything iff |version_matcher_high| is also empty.
  const char* version_matcher_low;
  // Matcher for highest version matched by this range (exclusive). May be empty
  // to match anything higher than |version_matcher_low|.
  const char* version_matcher_high;
  const char* min_version;  // Minimum secure version.
};

// Hard-coded definitions of plugin groups.
struct PluginGroupDefinition {
  const char* identifier;  // Unique identifier for this group.
  const char* name;  // Name of this group.
  const char* name_matcher;  // Substring matcher for the plugin name.
  const VersionRangeDefinition* versions;  // List of version ranges.
  const size_t num_versions;  // Size of the array |versions| points to.
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

  PluginGroup(const PluginGroup& other);

  ~PluginGroup();

  PluginGroup& operator=(const PluginGroup& other);

  // Configures the set of plugin name patterns for disabling plugins via
  // enterprise configuration management.
  static void SetPolicyDisabledPluginPatterns(const std::set<string16>& set);

  // Tests to see if a plugin is on the blacklist using its name as
  // the lookup key.
  static bool IsPluginNameDisabledByPolicy(const string16& plugin_name);

  // Tests to see if a plugin is on the blacklist using its path as
  // the lookup key.
  static bool IsPluginPathDisabledByPolicy(const FilePath& plugin_path);

  // Returns true if the given plugin matches this group.
  bool Match(const WebPluginInfo& plugin) const;

  // Adds the given plugin to this group. Provide the position of the
  // plugin as given by PluginList so we can display its priority.
  void AddPlugin(const WebPluginInfo& plugin, int position);

  bool IsEmpty() const;

  // Enables/disables this group. This enables/disables all plugins in the
  // group.
  void Enable(bool enable);

  // Returns whether the plugin group is enabled or not.
  bool Enabled() const { return enabled_; }

  // Returns a unique identifier for this group, if one is defined, or the empty
  // string otherwise.
  const std::string& identifier() const { return identifier_; }

  // Returns this group's name, or the filename without extension if the name
  // is empty.
  string16 GetGroupName() const;

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

  // Disables all plugins in this group that are older than the
  // minimum version.
  void DisableOutdatedPlugins();

  // Parse a version string as used by a plug-in. This method is more lenient
  // in accepting weird version strings than Version::GetFromString().
  static Version* CreateVersionFromString(const string16& version_string);

  std::vector<WebPluginInfo> web_plugin_infos() { return web_plugin_infos_; }

 private:
  typedef std::map<std::string, PluginGroup*> PluginMap;

  friend class PluginList;
  friend class PluginGroupTest;
  friend class ::TableModelArrayControllerTest;
  friend class ::PluginExceptionsTableModelTest;

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
  std::vector<int> web_plugin_positions_;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
