// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_group.h"

#include "base/linked_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugininfo.h"

const char* PluginGroup::kAdobeReader8GroupName = "Adobe Reader 8";
const char* PluginGroup::kAdobeReader9GroupName = "Adobe Reader 9";

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
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);
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

PluginGroup::PluginGroup(const string16& group_name,
                         const string16& name_matcher,
                         const std::string& version_range_low,
                         const std::string& version_range_high,
                         const std::string& min_version,
                         const std::string& update_url,
                         const std::string& identifier)
    : identifier_(identifier),
      group_name_(group_name),
      name_matcher_(name_matcher),
      version_range_low_str_(version_range_low),
      version_range_high_str_(version_range_high),
      update_url_(update_url),
      enabled_(false),
      min_version_str_(min_version),
      version_(Version::GetVersionFromString("0")) {
  if (!version_range_low.empty())
    version_range_low_.reset(Version::GetVersionFromString(version_range_low));
  if (!version_range_high.empty()) {
    version_range_high_.reset(
        Version::GetVersionFromString(version_range_high));
  }
  if (!min_version.empty())
    min_version_.reset(Version::GetVersionFromString(min_version));
}

void PluginGroup::InitFrom(const PluginGroup& other) {
  identifier_ = other.identifier_;
  group_name_ = other.group_name_;
  name_matcher_ = other.name_matcher_;
  version_range_low_str_ = other.version_range_low_str_;
  version_range_high_str_ = other.version_range_high_str_;
  version_range_low_.reset(
      Version::GetVersionFromString(version_range_low_str_));
  version_range_high_.reset(
      Version::GetVersionFromString(version_range_high_str_));
  description_ = other.description_;
  update_url_ = other.update_url_;
  enabled_ = other.enabled_;
  min_version_str_ = other.min_version_str_;
  min_version_.reset(Version::GetVersionFromString(min_version_str_));
  DCHECK_EQ(other.web_plugin_infos_.size(), other.web_plugin_positions_.size());
  for (size_t i = 0; i < other.web_plugin_infos_.size(); ++i)
    AddPlugin(other.web_plugin_infos_[i], other.web_plugin_positions_[i]);
  if (!version_.get())
    version_.reset(Version::GetVersionFromString("0"));
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
  return new PluginGroup(ASCIIToUTF16(definition.name),
                         ASCIIToUTF16(definition.name_matcher),
                         definition.version_matcher_low,
                         definition.version_matcher_high,
                         definition.min_version,
                         definition.update_url,
                         definition.identifier);
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
  return new PluginGroup(wpi.name, wpi.name, std::string(), std::string(),
                         std::string(), std::string(),
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

  if (version_range_low_.get() == NULL ||
      version_range_high_.get() == NULL) {
    return true;
  }

  // There's a version range, we must be in it.
  scoped_ptr<Version> plugin_version(
      Version::GetVersionFromString(UTF16ToWide(plugin.version)));
  if (plugin_version.get() == NULL) {
    // No version could be extracted, assume we don't match the range.
    return false;
  }

  // We match if we are in the range: [low, high)
  return (version_range_low_->CompareTo(*plugin_version) <= 0 &&
          version_range_high_->CompareTo(*plugin_version) > 0);
}

Version* PluginGroup::CreateVersionFromString(const string16& version_string) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::wstring version = UTF16ToWide(version_string);
  RemoveChars(version, L") ", &version);
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');

  return Version::GetVersionFromString(version);
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

// Returns true if the latest version of this plugin group is vulnerable.
bool PluginGroup::IsVulnerable() const {
  if (min_version_.get() == NULL || version_->GetString() == "0") {
    return false;
  }
  return version_->CompareTo(*min_version_) < 0;
}

void PluginGroup::DisableOutdatedPlugins() {
  if (!min_version_.get())
    return;

  description_ = string16();
  enabled_ = false;

  for (std::vector<WebPluginInfo>::iterator it =
           web_plugin_infos_.begin();
       it != web_plugin_infos_.end(); ++it) {
    scoped_ptr<Version> version(CreateVersionFromString(it->version));
    if (version.get() && version->CompareTo(*min_version_) < 0) {
      it->enabled = false;
      NPAPI::PluginList::Singleton()->DisablePlugin(it->path);
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
      NPAPI::PluginList::Singleton()->EnablePlugin(it->path);
      it->enabled = true;
      enabled_plugin_exists = true;
    } else {
      it->enabled = false;
      NPAPI::PluginList::Singleton()->DisablePlugin(it->path);
    }
  }
  enabled_ = enabled_plugin_exists;
}
