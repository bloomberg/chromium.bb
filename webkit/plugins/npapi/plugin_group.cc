// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "webkit/plugins/npapi/plugin_group.h"

#include "base/memory/linked_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/webplugininfo.h"

namespace webkit {
namespace npapi {

// static
const char PluginGroup::kAdobeReaderGroupName[] = "Adobe Acrobat";
const char PluginGroup::kAdobeReaderUpdateURL[] =
    "http://get.adobe.com/reader/";
const char PluginGroup::kJavaGroupName[] = "Java";
const char PluginGroup::kQuickTimeGroupName[] = "QuickTime";
const char PluginGroup::kShockwaveGroupName[] = "Shockwave";
const char PluginGroup::kRealPlayerGroupName[] = "RealPlayer";
const char PluginGroup::kSilverlightGroupName[] = "Silverlight";
const char PluginGroup::kWindowsMediaPlayerGroupName[] = "Windows Media Player";

VersionRange::VersionRange(const VersionRangeDefinition& definition)
    : low_str(definition.version_matcher_low),
      high_str(definition.version_matcher_high),
      min_str(definition.min_version),
      requires_authorization(definition.requires_authorization) {
  if (!low_str.empty())
    low.reset(Version::GetVersionFromString(low_str));
  if (!high_str.empty())
    high.reset(Version::GetVersionFromString(high_str));
  if (!min_str.empty())
    min.reset(Version::GetVersionFromString(min_str));
}

VersionRange::VersionRange(const VersionRange& other) {
  InitFrom(other);
}

VersionRange& VersionRange::operator=(const VersionRange& other) {
  InitFrom(other);
  return *this;
}

VersionRange::~VersionRange() {}

void VersionRange::InitFrom(const VersionRange& other) {
  low_str = other.low_str;
  high_str = other.high_str;
  min_str = other.min_str;
  low.reset(Version::GetVersionFromString(other.low_str));
  high.reset(Version::GetVersionFromString(other.high_str));
  min.reset(Version::GetVersionFromString(other.min_str));
  requires_authorization = other.requires_authorization;
}

PluginGroup::PluginGroup(const string16& group_name,
                         const string16& name_matcher,
                         const std::string& update_url,
                         const std::string& identifier)
    : identifier_(identifier),
      group_name_(group_name),
      name_matcher_(name_matcher),
      update_url_(update_url) {
}

void PluginGroup::InitFrom(const PluginGroup& other) {
  identifier_ = other.identifier_;
  group_name_ = other.group_name_;
  name_matcher_ = other.name_matcher_;
  update_url_ = other.update_url_;
  version_ranges_ = other.version_ranges_;
  web_plugin_infos_ = other.web_plugin_infos_;
}

PluginGroup::PluginGroup(const PluginGroup& other) {
  InitFrom(other);
}

PluginGroup& PluginGroup::operator=(const PluginGroup& other) {
  InitFrom(other);
  return *this;
}

/*static*/
PluginGroup* PluginGroup::FromPluginGroupDefinition(
    const PluginGroupDefinition& definition) {
  PluginGroup* group = new PluginGroup(ASCIIToUTF16(definition.name),
                                       ASCIIToUTF16(definition.name_matcher),
                                       definition.update_url,
                                       definition.identifier);
  for (size_t i = 0; i < definition.num_versions; ++i)
    group->version_ranges_.push_back(VersionRange(definition.versions[i]));
  return group;
}

PluginGroup::~PluginGroup() { }

/*static*/
std::string PluginGroup::GetIdentifier(const WebPluginInfo& wpi) {
#if defined(OS_POSIX)
  return wpi.path.BaseName().value();
#elif defined(OS_WIN)
  return base::SysWideToUTF8(wpi.path.BaseName().value());
#endif
}

/*static*/
std::string PluginGroup::GetLongIdentifier(const WebPluginInfo& wpi) {
#if defined(OS_POSIX)
  return wpi.path.value();
#elif defined(OS_WIN)
  return base::SysWideToUTF8(wpi.path.value());
#endif
}

/*static*/
PluginGroup* PluginGroup::FromWebPluginInfo(const WebPluginInfo& wpi) {
  // Create a matcher from the name of this plugin.
  return new PluginGroup(wpi.name, wpi.name, std::string(),
                         GetIdentifier(wpi));
}

bool PluginGroup::Match(const WebPluginInfo& plugin) const {
  if (name_matcher_.empty()) {
    return false;
  }

  // Look for the name matcher anywhere in the plugin name.
  if (plugin.name.find(name_matcher_) == string16::npos) {
    return false;
  }

  if (version_ranges_.empty()) {
    return true;
  }

  // There's at least one version range, the plugin's version must be in it.
  scoped_ptr<Version> plugin_version(CreateVersionFromString(plugin.version));
  if (plugin_version.get() == NULL) {
    // No version could be extracted, assume we don't match the range.
    return false;
  }

  // Match if the plugin is contained in any of the defined VersionRanges.
  for (size_t i = 0; i < version_ranges_.size(); ++i) {
    if (IsVersionInRange(*plugin_version, version_ranges_[i])) {
      return true;
    }
  }
  // None of the VersionRanges matched.
  return false;
}

/* static */
Version* PluginGroup::CreateVersionFromString(const string16& version_string) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::string version = UTF16ToASCII(version_string);
  RemoveChars(version, ") ", &version);
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  return Version::GetVersionFromString(version);
}

void PluginGroup::AddPlugin(const WebPluginInfo& plugin) {
  // Check if this group already contains this plugin.
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (FilePath::CompareEqualIgnoreCase(web_plugin_infos_[i].path.value(),
                                         plugin.path.value())) {
      return;
    }
  }
  web_plugin_infos_.push_back(plugin);
}

bool PluginGroup::RemovePlugin(const FilePath& filename) {
  bool did_remove = false;
  for (size_t i = 0; i < web_plugin_infos_.size();) {
    if (web_plugin_infos_[i].path == filename) {
      web_plugin_infos_.erase(web_plugin_infos_.begin() + i);
      did_remove = true;
    } else {
      i++;
    }
  }
  return did_remove;
}

string16 PluginGroup::GetGroupName() const {
  if (!group_name_.empty())
    return group_name_;
  DCHECK_EQ(1u, web_plugin_infos_.size());
  FilePath::StringType path =
      web_plugin_infos_[0].path.BaseName().RemoveExtension().value();
#if defined(OS_POSIX)
  return UTF8ToUTF16(path);
#elif defined(OS_WIN)
  return WideToUTF16(path);
#endif
}

bool PluginGroup::ContainsPlugin(const FilePath& path) const {
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (web_plugin_infos_[i].path == path)
      return true;
  }
  return false;
}

/*static*/
bool PluginGroup::IsVersionInRange(const Version& version,
                                   const VersionRange& range) {
  DCHECK(range.low.get() != NULL || range.high.get() == NULL)
      << "Lower bound of version range must be defined.";
  return (range.low.get() == NULL && range.high.get() == NULL) ||
         (range.low->CompareTo(version) <= 0 &&
             (range.high.get() == NULL || range.high->CompareTo(version) > 0));
}

/*static*/
bool PluginGroup::IsPluginOutdated(const Version& plugin_version,
                                   const VersionRange& version_range) {
  if (IsVersionInRange(plugin_version, version_range)) {
    if (version_range.min.get() &&
        plugin_version.CompareTo(*version_range.min) < 0) {
      return true;
    }
  }
  return false;
}

// Returns true if the latest version of this plugin group is vulnerable.
bool PluginGroup::IsVulnerable(const WebPluginInfo& plugin) const {
  scoped_ptr<Version> version(CreateVersionFromString(plugin.version));
  if (!version.get())
    return false;

  for (size_t i = 0; i < version_ranges_.size(); ++i) {
    if (IsPluginOutdated(*version, version_ranges_[i]))
      return true;
  }
  return false;
}

bool PluginGroup::RequiresAuthorization(const WebPluginInfo& plugin) const {
  scoped_ptr<Version> version(CreateVersionFromString(plugin.version));
  if (!version.get())
    return false;

  for (size_t i = 0; i < version_ranges_.size(); ++i) {
    if (IsVersionInRange(*version, version_ranges_[i]) &&
        version_ranges_[i].requires_authorization)
      return true;
  }
  return false;
}

bool PluginGroup::IsEmpty() const {
  return web_plugin_infos_.empty();
}

}  // namespace npapi
}  // namespace webkit
