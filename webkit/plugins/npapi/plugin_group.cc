// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

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

const char* PluginGroup::kAdobeReaderGroupName = "Adobe Acrobat";
const char* PluginGroup::kAdobeReaderUpdateURL = "http://get.adobe.com/reader/";
const char* PluginGroup::kJavaGroupName = "Java";
const char* PluginGroup::kQuickTimeGroupName = "QuickTime";
const char* PluginGroup::kShockwaveGroupName = "Shockwave";

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

VersionRange::VersionRange(VersionRangeDefinition definition)
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
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  return Version::GetVersionFromString(WideToASCII(version));
}

void PluginGroup::UpdateActivePlugin(const WebPluginInfo& plugin) {
  // A group is enabled if any of the files are enabled.
  if (IsPluginEnabled(plugin)) {
    // The description of the group needs update either when it's state is
    // about to change to enabled or if has never been set.
    if (!enabled_ || description_.empty())
      UpdateDescriptionAndVersion(plugin);
    // In case an enabled plugin has been added to a group that is currently
    // disabled then we should enable the group.
    if (!enabled_)
      enabled_ = true;
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

void PluginGroup::AddPlugin(const WebPluginInfo& plugin) {
  // Check if this group already contains this plugin.
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (FilePath::CompareEqualIgnoreCase(web_plugin_infos_[i].path.value(),
                                         plugin.path.value())) {
      return;
    }
  }
  web_plugin_infos_.push_back(plugin);
  UpdateActivePlugin(web_plugin_infos_.back());
}

bool PluginGroup::RemovePlugin(const FilePath& filename) {
  bool did_remove = false;
  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size();) {
    if (web_plugin_infos_[i].path == filename) {
      web_plugin_infos_.erase(web_plugin_infos_.begin() + i);
      did_remove = true;
    } else {
      UpdateActivePlugin(web_plugin_infos_[i]);
      i++;
    }
  }
  return did_remove;
}

bool PluginGroup::EnablePlugin(const FilePath& filename) {
  bool did_enable = false;
  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (web_plugin_infos_[i].path == filename)
      did_enable = Enable(&web_plugin_infos_[i], WebPluginInfo::USER_ENABLED);
    UpdateActivePlugin(web_plugin_infos_[i]);
  }
  return did_enable;
}

bool PluginGroup::DisablePlugin(const FilePath& filename) {
  bool did_disable = false;
  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    if (web_plugin_infos_[i].path == filename) {
      // We are only called for user intervention however we should respect a
      // policy that might as well be active on this plugin.
      did_disable = Disable(
          &web_plugin_infos_[i],
          IsPluginNameDisabledByPolicy(web_plugin_infos_[i].name) ?
              WebPluginInfo::USER_DISABLED_POLICY_DISABLED :
              WebPluginInfo::USER_DISABLED);
    }
    UpdateActivePlugin(web_plugin_infos_[i]);
  }
  return did_disable;
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
    DictionaryValue* plugin_file = new DictionaryValue();
    plugin_file->SetString("name", web_plugin_infos_[i].name);
    plugin_file->SetString("description", web_plugin_infos_[i].desc);
    plugin_file->SetString("path", web_plugin_infos_[i].path.value());
    plugin_file->SetString("version", web_plugin_infos_[i].version);
    bool plugin_disabled_by_policy = group_disabled_by_policy ||
        ((web_plugin_infos_[i].enabled & WebPluginInfo::POLICY_DISABLED) != 0);
    if (plugin_disabled_by_policy) {
      plugin_file->SetString("enabledMode", "disabledByPolicy");
    } else {
      all_plugins_disabled_by_policy = false;
      plugin_file->SetString(
          "enabledMode", IsPluginEnabled(web_plugin_infos_[i]) ?
              "enabled" : "disabledByUser");
    }

    ListValue* mime_types = new ListValue();
    const std::vector<WebPluginMimeType>& plugin_mime_types =
        web_plugin_infos_[i].mime_types;
    for (size_t j = 0; j < plugin_mime_types.size(); ++j) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString("mimeType", plugin_mime_types[j].mime_type);
      mime_type->SetString("description", plugin_mime_types[j].description);

      ListValue* file_extensions = new ListValue();
      const std::vector<std::string>& mime_file_extensions =
          plugin_mime_types[j].file_extensions;
      for (size_t k = 0; k < mime_file_extensions.size(); ++k)
        file_extensions->Append(new StringValue(mime_file_extensions[k]));
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

bool PluginGroup::RequiresAuthorization() const {
  for (size_t i = 0; i < version_ranges_.size(); ++i) {
    if (IsVersionInRange(*version_, version_ranges_[i]) &&
        version_ranges_[i].requires_authorization)
      return true;
  }
  return false;
}

bool PluginGroup::IsEmpty() const {
  return web_plugin_infos_.size() == 0;
}

void PluginGroup::DisableOutdatedPlugins() {
  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    scoped_ptr<Version> version(
        CreateVersionFromString(web_plugin_infos_[i].version));
    if (version.get()) {
      for (size_t j = 0; j < version_ranges_.size(); ++j) {
        if (IsPluginOutdated(*version, version_ranges_[j])) {
          Disable(&web_plugin_infos_[i], WebPluginInfo::USER_DISABLED);
          break;
        }
      }
    }
    UpdateActivePlugin(web_plugin_infos_[i]);
  }
}

bool PluginGroup::EnableGroup(bool enable) {
  bool group_disabled_by_policy = IsPluginNameDisabledByPolicy(group_name_);
  // We can't enable groups disabled by policy
  if (group_disabled_by_policy && enable)
    return false;

  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    bool policy_disabled =
        IsPluginNameDisabledByPolicy(web_plugin_infos_[i].name);
    if (enable && !policy_disabled) {
      Enable(&web_plugin_infos_[i], WebPluginInfo::USER_ENABLED);
    } else {
      Disable(&web_plugin_infos_[i],
              policy_disabled || group_disabled_by_policy ?
                  WebPluginInfo::POLICY_DISABLED :
                  WebPluginInfo::USER_DISABLED);
    }
    UpdateActivePlugin(web_plugin_infos_[i]);
  }
  return enabled_ == enable;
}

void PluginGroup::EnforceGroupPolicy() {
  bool group_disabled_by_policy = IsPluginNameDisabledByPolicy(group_name_);

  ResetGroupState();
  for (size_t i = 0; i < web_plugin_infos_.size(); ++i) {
    bool policy_disabled =
        IsPluginNameDisabledByPolicy(web_plugin_infos_[i].name) |
        group_disabled_by_policy;

    // TODO(pastarmovj): Add the code for enforcing enabled by policy...
    if (policy_disabled) {
      Disable(&web_plugin_infos_[i], WebPluginInfo::POLICY_DISABLED);
    // ...here would a else if (policy_enabled) { ... } be then.
    } else {
      Enable(&web_plugin_infos_[i], WebPluginInfo::POLICY_UNMANAGED);
    }
    UpdateActivePlugin(web_plugin_infos_[i]);
  }
}

void PluginGroup::ResetGroupState() {
  enabled_ = false;
  description_.clear();
  version_.reset(Version::GetVersionFromString("0"));
}

bool PluginGroup::Enable(WebPluginInfo* plugin,
                         int new_reason) {
  DCHECK(new_reason == WebPluginInfo::USER_ENABLED ||
         new_reason == WebPluginInfo::POLICY_UNMANAGED ||
         new_reason == WebPluginInfo::POLICY_ENABLED);
  // If we are only stripping the policy then mask the policy bits.
  if (new_reason == WebPluginInfo::POLICY_UNMANAGED) {
    plugin->enabled &= WebPluginInfo::USER_MASK;
    return true;
  }
  // If already enabled just upgrade the reason.
  if (IsPluginEnabled(*plugin)) {
    plugin->enabled |= new_reason;
    return true;
  } else {
    // Only changeable if not managed.
    if (plugin->enabled & WebPluginInfo::MANAGED_MASK)
      return false;
    plugin->enabled = new_reason;
  }
  return true;
}

bool PluginGroup::Disable(WebPluginInfo* plugin,
                          int new_reason) {
  DCHECK(new_reason == WebPluginInfo::USER_DISABLED ||
         new_reason == WebPluginInfo::POLICY_DISABLED ||
         new_reason == WebPluginInfo::USER_DISABLED_POLICY_DISABLED);
  // If already disabled just upgrade the reason.
  if (!IsPluginEnabled(*plugin)) {
    plugin->enabled |= new_reason;
    return true;
  } else {
    // Only changeable if not managed.
    if (plugin->enabled & WebPluginInfo::MANAGED_MASK)
      return false;
    plugin->enabled = new_reason;
  }
  return true;
}

}  // namespace npapi
}  // namespace webkit
