// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "webkit/plugins/webkit_plugins_export.h"
#include "webkit/plugins/webplugininfo.h"

class FilePath;
class PluginExceptionsTableModelTest;
class Version;

namespace webkit {
namespace npapi {

class PluginList;
class MockPluginList;

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
  // Unique identifier for this group.
  const char* identifier;
  // Name of this group.
  const char* name;
  // Substring matcher for the plugin name.
  const char* name_matcher;
  // List of version ranges.
  const VersionRangeDefinition* versions;
  // Size of the array |versions| points to.
  size_t num_versions;
};

// Run-time structure to hold version range information.
struct VersionRange {
 public:
  explicit VersionRange(const VersionRangeDefinition& definition);
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

class WEBKIT_PLUGINS_EXPORT PluginGroup {
 public:
  // Used by about:plugins to disable Reader plugin when internal PDF viewer is
  // enabled.
  static const char kAdobeReaderGroupName[];
  static const char kJavaGroupName[];
  static const char kQuickTimeGroupName[];
  static const char kShockwaveGroupName[];
  static const char kRealPlayerGroupName[];
  static const char kSilverlightGroupName[];
  static const char kWindowsMediaPlayerGroupName[];

  PluginGroup(const PluginGroup& other);

  ~PluginGroup();

  PluginGroup& operator=(const PluginGroup& other);

  // Returns true if the given plugin matches this group.
  bool Match(const webkit::WebPluginInfo& plugin) const;

  // Adds the given plugin to this group.
  void AddPlugin(const webkit::WebPluginInfo& plugin);

  // Removes a plugin from the group by its path.
  bool RemovePlugin(const FilePath& filename);

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

  // Checks whether a plugin exists in the group with the given path.
  bool ContainsPlugin(const FilePath& path) const;

  // Returns true if |plugin| in this group has known security problems.
  bool IsVulnerable(const WebPluginInfo& plugin) const;

  // Check if the group has no plugins. Could happen after a reload if the plug-
  // in has disappeared from the pc (or in the process of updating).
  bool IsEmpty() const;

  // Parse a version string as used by a plug-in. This method is more lenient
  // in accepting weird version strings than Version::GetFromString().
  static Version* CreateVersionFromString(const string16& version_string);

  const std::vector<webkit::WebPluginInfo>& web_plugin_infos() const {
    return web_plugin_infos_;
  }

  const std::vector<VersionRange>& version_ranges() const {
    return version_ranges_;
  }

 private:
  friend class MockPluginList;
  friend class PluginGroupTest;
  friend class PluginList;
  friend class ::PluginExceptionsTableModelTest;
  FRIEND_TEST_ALL_PREFIXES(PluginListTest, DisableOutdated);

  // Generates the (short) identifier string for the given plugin.
  static std::string GetIdentifier(const webkit::WebPluginInfo& wpi);

  // Generates the long identifier (based on the full file path) for the given
  // plugin, to be called when the short identifier is not unique.
  static std::string GetLongIdentifier(const webkit::WebPluginInfo& wpi);

  // Creates a PluginGroup from a PluginGroupDefinition. The caller takes
  // ownership of the created PluginGroup.
  static PluginGroup* FromPluginGroupDefinition(
      const PluginGroupDefinition& definition);

  // Creates a PluginGroup from a WebPluginInfo. The caller takes ownership of
  // the created PluginGroup.
  static PluginGroup* FromWebPluginInfo(const webkit::WebPluginInfo& wpi);

  // Returns |true| if |version| is contained in [low, high) of |range|.
  static bool IsVersionInRange(const Version& version,
                               const VersionRange& range);

  // Returns |true| iff |plugin_version| is both contained in |version_range|
  // and declared outdated (== vulnerable) by it.
  static bool IsPluginOutdated(const Version& plugin_version,
                               const VersionRange& version_range);

  PluginGroup(const string16& group_name,
              const string16& name_matcher,
              const std::string& identifier);

  void InitFrom(const PluginGroup& other);

  // Returns a non-const vector of all plugins in the group. This is only used
  // by PluginList.
  std::vector<webkit::WebPluginInfo>& GetPluginsContainer() {
    return web_plugin_infos_;
  }

  std::string identifier_;
  string16 group_name_;
  string16 name_matcher_;
  std::vector<VersionRange> version_ranges_;
  std::vector<webkit::WebPluginInfo> web_plugin_infos_;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_GROUP_H_
