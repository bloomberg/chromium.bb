// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/plugin_switches.h"

namespace webkit {
namespace npapi {

FilePath::CharType kDefaultPluginLibraryName[] =
    FILE_PATH_LITERAL("default_plugin");

// Some version ranges can be shared across operating systems. This should be
// done where possible to avoid duplication.
static const VersionRangeDefinition kFlashVersionRange[] = {
    { "", "", "10.1.102" }
};

// Similarly, try and share the group definition for plug-ins that are
// very consistent across OS'es.
static const PluginGroupDefinition kFlashDefinition = {
    "adobe-flash-player", "Flash", "Shockwave Flash", kFlashVersionRange,
    arraysize(kFlashVersionRange), "http://get.adobe.com/flashplayer/" };

#if defined(OS_MACOSX)
// Plugin Groups for Mac.
// Plugins are listed here as soon as vulnerabilities and solutions
// (new versions) are published.
// TODO(panayiotis): Get the Real Player version on Mac, somehow.
static const VersionRangeDefinition kQuicktimeVersionRange[] = {
    { "", "", "7.6.6" }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "13.0", "14.0", "13.3.0" }  // Snow Leopard
};
static const VersionRangeDefinition kSilverlightVersionRange[] = {
    { "0", "4", "3.0.50106.0" },
    { "4", "5", "" }
};
static const VersionRangeDefinition kFlip4MacVersionRange[] = {
    { "", "", "2.2.1" }
};
static const VersionRangeDefinition kShockwaveVersionRange[] = {
    { "",  "", "11.5.9.615" }
};
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", "Quicktime", "QuickTime Plug-in", kQuicktimeVersionRange,
    arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", "Java", "Java", kJavaVersionRange,
    arraysize(kJavaVersionRange), "http://support.apple.com/kb/HT1338" },
  { "silverlight", "Silverlight", "Silverlight", kSilverlightVersionRange,
    arraysize(kSilverlightVersionRange),
    "http://www.microsoft.com/getsilverlight/" },
  { "flip4mac", "Flip4Mac", "Flip4Mac", kFlip4MacVersionRange,
    arraysize(kFlip4MacVersionRange),
    "http://www.telestream.net/flip4mac-wmv/overview.htm" },
  { "shockwave", "Shockwave", "Shockwave for Director", kShockwaveVersionRange,
    arraysize(kShockwaveVersionRange),
    "http://www.adobe.com/shockwave/download/" }
};

#elif defined(OS_WIN)
// TODO(panayiotis): We should group "RealJukebox NS Plugin" with the rest of
// the RealPlayer files.
static const VersionRangeDefinition kQuicktimeVersionRange[] = {
    { "", "", "7.6.8" }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "7", "6.0.220" }  // "220" is not a typo.
};
static const VersionRangeDefinition kAdobeReaderVersionRange[] = {
    { "10", "11", "" },
    { "9", "10", "9.4.1" },
    { "0", "9", "8.2.5" }
};
static const VersionRangeDefinition kSilverlightVersionRange[] = {
    { "0", "4", "3.0.50106.0" },
    { "4", "5", "" }
};
static const VersionRangeDefinition kShockwaveVersionRange[] = {
    { "", "", "11.5.9.615" }
};
static const VersionRangeDefinition kDivXVersionRange[] = {
    { "", "", "1.4.3.4" }
};
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", "Quicktime", "QuickTime Plug-in", kQuicktimeVersionRange,
    arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", "Java 6", "Java", kJavaVersionRange,
    arraysize(kJavaVersionRange), "http://www.java.com/" },
  { "adobe-reader", PluginGroup::kAdobeReaderGroupName, "Adobe Acrobat",
    kAdobeReaderVersionRange, arraysize(kAdobeReaderVersionRange),
    "http://get.adobe.com/reader/" },
  { "silverlight", "Silverlight", "Silverlight", kSilverlightVersionRange,
    arraysize(kSilverlightVersionRange),
    "http://www.microsoft.com/getsilverlight/" },
  { "shockwave", "Shockwave", "Shockwave for Director", kShockwaveVersionRange,
    arraysize(kShockwaveVersionRange),
    "http://www.adobe.com/shockwave/download/" },
  { "divx-player", "DivX Player", "DivX Web Player", kDivXVersionRange,
    arraysize(kDivXVersionRange),
    "http://download.divx.com/divx/autoupdate/player/"
    "DivXWebPlayerInstaller.exe" },
  // These are here for grouping, no vulnerabilities known.
  { "windows-media-player", "Windows Media Player", "Windows Media Player",
    NULL, 0, "" },
  { "microsoft-office", "Microsoft Office", "Microsoft Office",
    NULL, 0, "" },
  // TODO(panayiotis): The vulnerable versions are
  //  (v >=  6.0.12.1040 && v <= 6.0.12.1663)
  //  || v == 6.0.12.1698  || v == 6.0.12.1741
  { "realplayer", "RealPlayer", "RealPlayer", NULL, 0,
    "www.real.com/realplayer/downloads" },
};

#else
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "1.7", "1.6.0.22" }
};

static const VersionRangeDefinition kRedhatIcedTeaVersionRange[] = {
    { "0", "1.9", "1.8.3" },
    { "1.9", "1.10", "1.9.2" },
};

static const PluginGroupDefinition kGroupDefinitions[] = {
  // Flash on Linux is significant because there isn't yet a built-in Flash
  // plug-in on the Linux 64-bit version of Chrome.
  kFlashDefinition,
  { "java-runtime-environment", "Java 6", "Java", kJavaVersionRange,
    arraysize(kJavaVersionRange),
    "http://www.java.com/en/download/manual.jsp" },
  { "redhat-icetea-java", "IcedTea", "IcedTea", kRedhatIcedTeaVersionRange,
    arraysize(kRedhatIcedTeaVersionRange),
    "http://www.linuxsecurity.com/content/section/3/170/" },
};
#endif

// static
const PluginGroupDefinition* PluginList::GetPluginGroupDefinitions() {
  return kGroupDefinitions;
}

// static
size_t PluginList::GetPluginGroupDefinitionsSize() {
  // TODO(viettrungluu): |arraysize()| doesn't work with zero-size arrays.
  return ARRAYSIZE_UNSAFE(kGroupDefinitions);
}

base::LazyInstance<PluginList> g_singleton(base::LINKER_INITIALIZED);

// static
PluginList* PluginList::Singleton() {
  return g_singleton.Pointer();
}

// static
bool PluginList::DebugPluginLoading() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDebugPluginLoading);
}

bool PluginList::PluginsLoaded() {
  AutoLock lock(lock_);
  return plugins_loaded_;
}

void PluginList::RefreshPlugins() {
  AutoLock lock(lock_);
  plugins_need_refresh_ = true;
}

void PluginList::AddExtraPluginPath(const FilePath& plugin_path) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  AutoLock lock(lock_);
  extra_plugin_paths_.push_back(plugin_path);
#endif
}

void PluginList::RemoveExtraPluginPath(const FilePath& plugin_path) {
  AutoLock lock(lock_);
  std::vector<FilePath>::iterator it =
      std::find(extra_plugin_paths_.begin(), extra_plugin_paths_.end(),
                plugin_path);
  if (it != extra_plugin_paths_.end())
    extra_plugin_paths_.erase(it);
}

void PluginList::AddExtraPluginDir(const FilePath& plugin_dir) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  AutoLock lock(lock_);
  extra_plugin_dirs_.push_back(plugin_dir);
#endif
}

void PluginList::RegisterInternalPlugin(const WebPluginInfo& info) {
  PluginEntryPoints entry_points = {0};
  InternalPlugin plugin = { info, entry_points };

  AutoLock lock(lock_);
  internal_plugins_.push_back(plugin);
}

void PluginList::RegisterInternalPlugin(const FilePath& filename,
                                        const std::string& name,
                                        const std::string& description,
                                        const std::string& mime_type_str,
                                        const PluginEntryPoints& entry_points) {
  InternalPlugin plugin;
  plugin.info.path = filename;
  plugin.info.name = ASCIIToUTF16(name);
  plugin.info.version = ASCIIToUTF16("1");
  plugin.info.desc = ASCIIToUTF16(description);
  plugin.info.enabled = true;

  WebPluginMimeType mime_type;
  mime_type.mime_type = mime_type_str;
  plugin.info.mime_types.push_back(mime_type);

  plugin.entry_points = entry_points;

  AutoLock lock(lock_);
  internal_plugins_.push_back(plugin);
}

void PluginList::UnregisterInternalPlugin(const FilePath& path) {
  AutoLock lock(lock_);
  for (size_t i = 0; i < internal_plugins_.size(); i++) {
    if (internal_plugins_[i].info.path == path) {
      internal_plugins_.erase(internal_plugins_.begin() + i);
      return;
    }
  }
  NOTREACHED();
}

bool PluginList::ReadPluginInfo(const FilePath& filename,
                                WebPluginInfo* info,
                                const PluginEntryPoints** entry_points) {
  {
    AutoLock lock(lock_);
    for (size_t i = 0; i < internal_plugins_.size(); ++i) {
      if (filename == internal_plugins_[i].info.path) {
        *entry_points = &internal_plugins_[i].entry_points;
        *info = internal_plugins_[i].info;
        return true;
      }
    }
  }

  // Not an internal plugin.
  *entry_points = NULL;

  return PluginLib::ReadWebPluginInfo(filename, info);
}

// static
bool PluginList::ParseMimeTypes(
    const std::string& mime_types_str,
    const std::string& file_extensions_str,
    const string16& mime_type_descriptions_str,
    std::vector<WebPluginMimeType>* parsed_mime_types) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<string16> descriptions;
  base::SplitString(mime_types_str, '|', &mime_types);
  base::SplitString(file_extensions_str, '|', &file_extensions);
  base::SplitString(mime_type_descriptions_str, '|', &descriptions);

  parsed_mime_types->clear();

  if (mime_types.empty())
    return false;

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      base::SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // On Windows, the description likely has a list of file extensions
      // embedded in it (e.g. "SurfWriter file (*.swr)"). Remove an extension
      // list from the description if it is present.
      size_t ext = mime_type.description.find(ASCIIToUTF16("(*"));
      if (ext != string16::npos) {
        if (ext > 1 && mime_type.description[ext - 1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    parsed_mime_types->push_back(mime_type);
  }

  return true;
}

PluginList::PluginList()
    : plugins_loaded_(false),
      plugins_need_refresh_(false),
      disable_outdated_plugins_(false),
      next_priority_(0) {
  PlatformInit();
  AddHardcodedPluginGroups();
}

bool PluginList::ShouldDisableGroup(const string16& group_name) {
  AutoLock lock(lock_);
  if (PluginGroup::IsPluginNameDisabledByPolicy(group_name)) {
    disabled_groups_.insert(group_name);
    return true;
  }
  return disabled_groups_.count(group_name) > 0;
}

void PluginList::LoadPlugins(bool refresh) {
  // Don't want to hold the lock while loading new plugins, so we don't block
  // other methods if they're called on other threads.
  std::vector<FilePath> extra_plugin_paths;
  std::vector<FilePath> extra_plugin_dirs;
  std::vector<InternalPlugin> internal_plugins;
  {
    AutoLock lock(lock_);
    if (plugins_loaded_ && !refresh && !plugins_need_refresh_)
      return;

    // Clear the refresh bit now, because it might get set again before we
    // reach the end of the method.
    plugins_need_refresh_ = false;
    extra_plugin_paths = extra_plugin_paths_;
    extra_plugin_dirs = extra_plugin_dirs_;
    internal_plugins = internal_plugins_;
  }

  std::vector<WebPluginInfo> new_plugins;
  std::set<FilePath> visited_plugins;

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  // Load internal plugins first so that, if both an internal plugin and a
  // "discovered" plugin want to handle the same type, the internal plugin
  // will have precedence.
  for (size_t i = 0; i < internal_plugins.size(); ++i) {
    if (internal_plugins[i].info.path.value() == kDefaultPluginLibraryName)
      continue;
    LoadPlugin(internal_plugins[i].info.path, &new_plugins);
  }

  for (size_t i = 0; i < extra_plugin_paths.size(); ++i) {
    const FilePath& path = extra_plugin_paths[i];
    if (visited_plugins.find(path) != visited_plugins.end())
      continue;
    LoadPlugin(path, &new_plugins);
    visited_plugins.insert(path);
  }

  for (size_t i = 0; i < extra_plugin_dirs.size(); ++i) {
    LoadPluginsFromDir(extra_plugin_dirs[i], &new_plugins, &visited_plugins);
  }

  for (size_t i = 0; i < directories_to_scan.size(); ++i) {
    LoadPluginsFromDir(directories_to_scan[i], &new_plugins, &visited_plugins);
  }

#if defined(OS_WIN)
  LoadPluginsFromRegistry(&new_plugins, &visited_plugins);
#endif

  // Load the default plugin last.
  if (webkit_glue::IsDefaultPluginEnabled())
    LoadPlugin(FilePath(kDefaultPluginLibraryName), &new_plugins);

  // Disable all of the plugins and plugin groups that are disabled by policy.
  // There's currenly a bug that makes it impossible to correctly re-enable
  // plugins or plugin-groups to their original, "pre-policy" state, so
  // plugins and groups are only changed to a more "safe" state after a policy
  // change, i.e. from enabled to disabled. See bug 54681.
  for (PluginGroup::PluginMap::iterator it = plugin_groups_.begin();
       it != plugin_groups_.end(); ++it) {
    PluginGroup* group = it->second;
    string16 group_name = group->GetGroupName();
    if (ShouldDisableGroup(group_name)) {
      group->Enable(false);
    }

    if (disable_outdated_plugins_) {
      group->DisableOutdatedPlugins();
    }
    if (!group->Enabled()) {
      AutoLock lock(lock_);
      disabled_groups_.insert(group_name);
    }
  }

  // Only update the data now since loading plugins can take a while.
  AutoLock lock(lock_);

  plugins_ = new_plugins;
  plugins_loaded_ = true;
}

void PluginList::LoadPlugin(const FilePath& path,
                            std::vector<WebPluginInfo>* plugins) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Loading plugin " << path.value();

  WebPluginInfo plugin_info;
  const PluginEntryPoints* entry_points;

  if (!ReadPluginInfo(path, &plugin_info, &entry_points))
    return;

  if (!ShouldLoadPlugin(plugin_info, plugins))
    return;

  if (path.value() != kDefaultPluginLibraryName
#if defined(OS_WIN) && !defined(NDEBUG)
      && path.BaseName().value() != L"npspy.dll"  // Make an exception for NPSPY
#endif
      ) {
    for (size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
      // TODO: don't load global handlers for now.
      // WebKit hands to the Plugin before it tries
      // to handle mimeTypes on its own.
      const std::string &mime_type = plugin_info.mime_types[i].mime_type;
      if (mime_type == "*" )
        return;
    }
  }

  // Mark disabled plugins as such. (This has to happen before calling
  // |AddToPluginGroups(plugin_info)|.)
  if (disabled_plugins_.count(plugin_info.path)) {
    plugin_info.enabled = false;
  } else {
    plugin_info.enabled = true;
  }

  AutoLock lock(lock_);
  plugins->push_back(plugin_info);
  AddToPluginGroups(plugin_info);
}

bool PluginList::SupportsType(const WebPluginInfo& info,
                              const std::string &mime_type,
                              bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = info.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && mime_info.mime_type == "*") {
        continue;
      }
      return true;
    }
  }
  return false;
}

bool PluginList::SupportsExtension(const WebPluginInfo& info,
                                   const std::string &extension,
                                   std::string* actual_mime_type) {
  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_type = info.mime_types[i];
    for (size_t j = 0; j < mime_type.file_extensions.size(); ++j) {
      if (mime_type.file_extensions[j] == extension) {
        if (actual_mime_type)
          *actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }

  return false;
}


void PluginList::GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  LoadPlugins(refresh);

  AutoLock lock(lock_);
  *plugins = plugins_;
}

void PluginList::GetEnabledPlugins(bool refresh,
                                   std::vector<WebPluginInfo>* plugins) {
  LoadPlugins(refresh);

  plugins->clear();
  AutoLock lock(lock_);
  for (std::vector<WebPluginInfo>::const_iterator it = plugins_.begin();
       it != plugins_.end();
       ++it) {
    if (it->enabled)
      plugins->push_back(*it);
  }
}

void PluginList::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<WebPluginInfo>* info,
    std::vector<std::string>* actual_mime_types) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));
  DCHECK(info);

  LoadPlugins(false);
  AutoLock lock(lock_);
  info->clear();
  if (actual_mime_types)
    actual_mime_types->clear();

  std::set<FilePath> visited_plugins;

  // Add in enabled plugins by mime type.
  WebPluginInfo default_plugin;
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].enabled &&
        SupportsType(plugins_[i], mime_type, allow_wildcard)) {
      FilePath path = plugins_[i].path;
      if (path.value() != kDefaultPluginLibraryName &&
          visited_plugins.insert(path).second) {
        info->push_back(plugins_[i]);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
    }
  }

  // Add in enabled plugins by url.
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot != std::string::npos) {
    std::string extension = StringToLowerASCII(std::string(path, last_dot+1));
    std::string actual_mime_type;
    for (size_t i = 0; i < plugins_.size(); ++i) {
      if (plugins_[i].enabled &&
          SupportsExtension(plugins_[i], extension, &actual_mime_type)) {
        FilePath path = plugins_[i].path;
        if (path.value() != kDefaultPluginLibraryName &&
            visited_plugins.insert(path).second) {
          info->push_back(plugins_[i]);
          if (actual_mime_types)
            actual_mime_types->push_back(actual_mime_type);
        }
      }
    }
  }

  // Add in disabled plugins by mime type.
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (!plugins_[i].enabled &&
        SupportsType(plugins_[i], mime_type, allow_wildcard)) {
      FilePath path = plugins_[i].path;
      if (path.value() != kDefaultPluginLibraryName &&
          visited_plugins.insert(path).second) {
        info->push_back(plugins_[i]);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
    }
  }

  // Add the default plugin at the end if it supports the mime type given,
  // and the default plugin is enabled.
  if (!plugins_.empty() && webkit_glue::IsDefaultPluginEnabled()) {
    const WebPluginInfo& default_info = plugins_.back();
    if (SupportsType(default_info, mime_type, allow_wildcard)) {
      info->push_back(default_info);
      if (actual_mime_types)
        actual_mime_types->push_back(mime_type);
    }
  }
}

bool PluginList::GetPluginInfo(const GURL& url,
                               const std::string& mime_type,
                               bool allow_wildcard,
                               WebPluginInfo* info,
                               std::string* actual_mime_type) {
  DCHECK(info);
  std::vector<WebPluginInfo> info_list;

  // GetPluginInfoArray has slightly less work to do if we can pass
  // NULL for the mime type list...
  if (actual_mime_type) {
    std::vector<std::string> mime_type_list;
    GetPluginInfoArray(
        url, mime_type, allow_wildcard, &info_list, &mime_type_list);
    if (!info_list.empty()) {
      *info = info_list[0];
      *actual_mime_type = mime_type_list[0];
      return true;
    }
  } else {
    GetPluginInfoArray(url, mime_type, allow_wildcard, &info_list, NULL);
    if (!info_list.empty()) {
      *info = info_list[0];
      return true;
    }
  }
  return false;
}

bool PluginList::GetPluginInfoByPath(const FilePath& plugin_path,
                                     WebPluginInfo* info) {
  LoadPlugins(false);
  AutoLock lock(lock_);
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (plugins_[i].path == plugin_path) {
      *info = plugins_[i];
      return true;
    }
  }

  return false;
}

void PluginList::GetPluginGroups(
    bool load_if_necessary,
    std::vector<PluginGroup>* plugin_groups) {
  if (load_if_necessary)
    LoadPlugins(false);
  plugin_groups->clear();
  for (PluginGroup::PluginMap::const_iterator it = plugin_groups_.begin();
       it != plugin_groups_.end(); ++it) {
    if (!it->second->IsEmpty())
      plugin_groups->push_back(*it->second);
  }
}

const PluginGroup* PluginList::GetPluginGroup(
    const WebPluginInfo& web_plugin_info) {
  AutoLock lock(lock_);
  return AddToPluginGroups(web_plugin_info);
}

string16 PluginList::GetPluginGroupName(std::string identifier) {
  PluginGroup::PluginMap::iterator it = plugin_groups_.find(identifier);
  if (it == plugin_groups_.end()) {
    return string16();
  }
  return it->second->GetGroupName();
}

std::string PluginList::GetPluginGroupIdentifier(
    const WebPluginInfo& web_plugin_info) {
  AutoLock lock(lock_);
  PluginGroup* group = AddToPluginGroups(web_plugin_info);
  return group->identifier();
}

void PluginList::AddHardcodedPluginGroups() {
  AutoLock lock(lock_);
  const PluginGroupDefinition* definitions = GetPluginGroupDefinitions();
  for (size_t i = 0; i < GetPluginGroupDefinitionsSize(); ++i) {
    PluginGroup* definition_group = PluginGroup::FromPluginGroupDefinition(
        definitions[i]);
    std::string identifier = definition_group->identifier();
    DCHECK(plugin_groups_.find(identifier) == plugin_groups_.end());
    plugin_groups_.insert(std::make_pair(identifier, definition_group));
  }
}

PluginGroup* PluginList::AddToPluginGroups(
    const WebPluginInfo& web_plugin_info) {
  PluginGroup* group = NULL;
  for (PluginGroup::PluginMap::iterator it = plugin_groups_.begin();
       it != plugin_groups_.end(); ++it) {
    if (it->second->Match(web_plugin_info))
      group = it->second;
  }
  if (!group) {
    group = PluginGroup::FromWebPluginInfo(web_plugin_info);
    std::string identifier = group->identifier();
    // If the identifier is not unique, use the full path. This means that we
    // probably won't be able to search for this group by identifier, but at
    // least it's going to be in the set of plugin groups, and if there
    // is already a plug-in with the same filename, it's probably going to
    // handle the same MIME types (and it has a higher priority), so this one
    // is not going to run anyway.
    if (plugin_groups_.find(identifier) != plugin_groups_.end())
      identifier = PluginGroup::GetLongIdentifier(web_plugin_info);
    DCHECK(plugin_groups_.find(identifier) == plugin_groups_.end());
    plugin_groups_.insert(std::make_pair(identifier, group));
  }
  group->AddPlugin(web_plugin_info, next_priority_++);
  return group;
}

bool PluginList::EnablePlugin(const FilePath& filename) {
  AutoLock lock(lock_);

  bool did_enable = false;

  std::set<FilePath>::iterator entry = disabled_plugins_.find(filename);
  if (entry == disabled_plugins_.end())
    return did_enable;  // Early exit if plugin not in disabled list.

  disabled_plugins_.erase(entry);  // Remove from disabled list.

  // Set enabled flags if necessary.
  for (std::vector<WebPluginInfo>::iterator it = plugins_.begin();
       it != plugins_.end();
       ++it) {
    if (it->path == filename) {
      DCHECK(!it->enabled);  // Should have been disabled.
      it->enabled = true;
      did_enable = true;
    }
  }

  return did_enable;
}

bool PluginList::DisablePlugin(const FilePath& filename) {
  AutoLock lock(lock_);

  bool did_disable = false;

  if (disabled_plugins_.find(filename) != disabled_plugins_.end())
    return did_disable;  // Early exit if plugin already in disabled list.

  disabled_plugins_.insert(filename);  // Add to disabled list.

  // Unset enabled flags if necessary.
  for (std::vector<WebPluginInfo>::iterator it = plugins_.begin();
       it != plugins_.end();
       ++it) {
    if (it->path == filename) {
      DCHECK(it->enabled);  // Should have been enabled.
      it->enabled = false;
      did_disable = true;
    }
  }

  return did_disable;
}

bool PluginList::EnableGroup(bool enable, const string16& group_name) {
  bool did_change = false;
  {
    AutoLock lock(lock_);

    std::set<string16>::iterator entry = disabled_groups_.find(group_name);
    if (enable) {
      if (entry == disabled_groups_.end())
        return did_change;  // Early exit if group not in disabled list.
      disabled_groups_.erase(entry);  // Remove from disabled list.
    } else {
      if (entry != disabled_groups_.end())
        return did_change;  // Early exit if group already in disabled list.
      disabled_groups_.insert(group_name);
    }
  }

  for (PluginGroup::PluginMap::iterator it = plugin_groups_.begin();
       it != plugin_groups_.end(); ++it) {
    if (it->second->GetGroupName() == group_name) {
      if (it->second->Enabled() != enable) {
        it->second->Enable(enable);
        did_change = true;
        break;
      }
    }
  }

  return did_change;
}

void PluginList::DisableOutdatedPluginGroups() {
  disable_outdated_plugins_ = true;
}

PluginList::~PluginList() {
  Shutdown();
}

void PluginList::Shutdown() {
  STLDeleteContainerPairSecondPointers(plugin_groups_.begin(),
                                       plugin_groups_.end());
}

}  // namespace npapi
}  // namespace webkit
