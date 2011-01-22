// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_group.h"

#include "base/linked_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

namespace webkit {
namespace npapi {

const char* PluginGroup::kAdobeReaderGroupName = "Adobe Reader";
const char* PluginGroup::kAdobeReaderUpdateURL = "http://get.adobe.com/reader/";

/*static*/
std::set<string16>* PluginGroup::policy_disabled_plugin_patterns_;

/*static*/
void PluginGroup::SetPolicyDisabledPluginPatterns(
    const std::set<string16>& set) {
  if (!policy_disabled_plugin_patterns_)
    policy_disabled_plugin_patterns_ = new std::set<string16>(set);
  else
    *policy_disabled_plugin_patterns_ = set;
}

/*static*/
bool PluginGroup::IsPluginNameDisabledByPolicy(const string16& plugin_name) {
  if (!policy_disabled_plugin_patterns_)
    return false;

  std::set<string16>::const_iterator pattern(
      policy_disabled_plugin_patterns_->begin());
  while (pattern != policy_disabled_plugin_patterns_->end()) {
    if (MatchPattern(plugin_name, *pattern))
      return true;
    ++pattern;
  }

  return false;
}

/*static*/
bool PluginGroup::IsPluginPathDisabledByPolicy(const FilePath& plugin_path) {
  std::vector<WebPluginInfo> plugins;
  PluginList::Singleton()->GetPlugins(false, &plugins);
  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    if (FilePath::CompareEqualIgnoreCase(it->path.value(),
        plugin_path.value()) && IsPluginNameDisabledByPolicy(it->name)) {
      return true;
    }
  }
  return false;
}

VersionRange::VersionRange(VersionRangeDefinition definition)
    : low_str(definition.version_matcher_low),
      high_str(definition.version_matcher_high),
      min_str(definition.min_version) {
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
}

PluginGroup::PluginGroup(const string16& group_name,
                         const string16& name_matcher,
                         const std::string& update_url,
                         const std::string& identifier)
    : identifier_(identifier),
      group_name_(group_name),
      name_matcher_(name_matcher),
      update_url_(update_url),
      enabled_(false),
      version_(Version::GetVersionFromString("0")) {
}

void PluginGroup::InitFrom(const PluginGroup& other) {
  identifier_ = other.identifier_;
  group_name_ = other.group_name_;
  name_matcher_ = other.name_matcher_;
  description_ = other.description_;
  update_url_ = other.update_url_;
  enabled_ = other.enabled_;
  version_ranges_ = other.version_ranges_;
  version_.reset(other.version_->Clone());
  web_plugin_infos_ = other.web_plugin_infos_;
  web_plugin_positions_ = other.web_plugin_positions_;
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
  std::wstring version = UTF16ToWide(version_string);
  RemoveChars(version, L") ", &version);
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  return Version::GetVersionFromString(WideToASCII(version));
}

void PluginGroup::UpdateActivePlugin(const WebPluginInfo& plugin) {
  // A group is enabled if any of the files are enabled.
  if (plugin.enabled) {
    if (!enabled_) {
      // If this is the first enabled plugin, use its description.
      enabled_ = true;
      UpdateDescriptionAndVersion(plugin);
    }
  } else {
    // If this is the first plugin and it's disabled,
    // use its description for now.
    if (description_.empty())
      UpdateDescriptionAndVersion(plugin);
  }
}

void PluginGroup::UpdateDescriptionAndVersion(const WebPluginInfo& plugin) {
  description_ = plugin.desc;
  if (Version* new_version = CreateVersionFromString(plugin.version))
    version_.reset(new_version);
  else
    version_.reset(Version::GetVersionFromString("0"));
}

void PluginGroup::AddPlugin(const WebPluginInfo& plugin, int position) {
  // Check if this group already contains this plugin.
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (web_plugin_infos_[i].name == plugin.name &&
        web_plugin_infos_[i].version == plugin.version &&
        FilePath::CompareEqualIgnoreCase(web_plugin_infos_[i].path.value(),
                                         plugin.path.value())) {
      return;
    }
  }
  web_plugin_infos_.push_back(plugin);
  // The position of this plugin relative to the global list of plugins.
  web_plugin_positions_.push_back(position);
  UpdateActivePlugin(plugin);
}

bool PluginGroup::IsEmpty() const {
  return web_plugin_infos_.empty();
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

DictionaryValue* PluginGroup::GetSummary() const {
  DictionaryValue* result = new DictionaryValue();
  result->SetString("name", GetGroupName());
  result->SetBoolean("enabled", enabled_);
  return result;
}

DictionaryValue* PluginGroup::GetDataForUI() const {
  string16 name = GetGroupName();
  DictionaryValue* result = new DictionaryValue();
  result->SetString("name", name);
  result->SetString("description", description_);
  result->SetString("version", version_->GetString());
  result->SetString("update_url", update_url_);
  result->SetBoolean("critical", IsVulnerable());

  bool group_disabled_by_policy = IsPluginNameDisabledByPolicy(name);
  ListValue* plugin_files = new ListValue();
  bool all_plugins_disabled_by_policy = true;
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    const WebPluginInfo& web_plugin = web_plugin_infos_[i];
    int priority = web_plugin_positions_[i];
    DictionaryValue* plugin_file = new DictionaryValue();
    plugin_file->SetString("name", web_plugin.name);
    plugin_file->SetString("description", web_plugin.desc);
    plugin_file->SetString("path", web_plugin.path.value());
    plugin_file->SetString("version", web_plugin.version);
    bool plugin_disabled_by_policy = group_disabled_by_policy ||
        IsPluginNameDisabledByPolicy(web_plugin.name);
    if (plugin_disabled_by_policy) {
      plugin_file->SetString("enabledMode", "disabledByPolicy");
    } else {
      all_plugins_disabled_by_policy = false;
      plugin_file->SetString("enabledMode",
                             web_plugin.enabled ? "enabled" : "disabledByUser");
    }
    plugin_file->SetInteger("priority", priority);

    ListValue* mime_types = new ListValue();
    for (std::vector<WebPluginMimeType>::const_iterator type_it =
         web_plugin.mime_types.begin();
         type_it != web_plugin.mime_types.end();
         ++type_it) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString("mimeType", type_it->mime_type);
      mime_type->SetString("description", type_it->description);

      ListValue* file_extensions = new ListValue();
      for (std::vector<std::string>::const_iterator ext_it =
           type_it->file_extensions.begin();
           ext_it != type_it->file_extensions.end();
           ++ext_it) {
        file_extensions->Append(new StringValue(*ext_it));
      }
      mime_type->Set("fileExtensions", file_extensions);

      mime_types->Append(mime_type);
    }
    plugin_file->Set("mimeTypes", mime_types);

    plugin_files->Append(plugin_file);
  }

  if (group_disabled_by_policy || all_plugins_disabled_by_policy) {
    result->SetString("enabledMode", "disabledByPolicy");
  } else {
    result->SetString("enabledMode", enabled_ ? "enabled" : "disabledByUser");
  }
  result->Set("plugin_files", plugin_files);

  return result;
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
bool PluginGroup::IsVulnerable() const {
  for (size_t i = 0; i < version_ranges_.size(); ++i) {
    if (IsPluginOutdated(*version_, version_ranges_[i]))
      return true;
  }
  return false;
}

void PluginGroup::DisableOutdatedPlugins() {
  description_ = string16();
  enabled_ = false;

  for (std::vector<WebPluginInfo>::iterator it =
           web_plugin_infos_.begin();
       it != web_plugin_infos_.end(); ++it) {
    scoped_ptr<Version> version(CreateVersionFromString(it->version));
    if (version.get()) {
      for (size_t i = 0; i < version_ranges_.size(); ++i) {
        if (IsPluginOutdated(*version, version_ranges_[i])) {
          it->enabled = false;
          PluginList::Singleton()->DisablePlugin(it->path);
        }
      }
    }
    UpdateActivePlugin(*it);
  }
}

void PluginGroup::Enable(bool enable) {
  bool enabled_plugin_exists = false;
  for (std::vector<WebPluginInfo>::iterator it =
       web_plugin_infos_.begin();
       it != web_plugin_infos_.end(); ++it) {
    if (enable && !IsPluginNameDisabledByPolicy(it->name)) {
      PluginList::Singleton()->EnablePlugin(it->path);
      it->enabled = true;
      enabled_plugin_exists = true;
    } else {
      it->enabled = false;
      PluginList::Singleton()->DisablePlugin(it->path);
    }
  }
  enabled_ = enabled_plugin_exists;
}

}  // namespace npapi
}  // namespace webkit
