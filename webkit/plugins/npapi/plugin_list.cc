// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
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
    { "", "", "10.1.102", false }
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
    { "", "", "7.6.6", true }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "13.0", "14.0", "13.3.0", true }  // Snow Leopard
};
static const VersionRangeDefinition kSilverlightVersionRange[] = {
    { "0", "4", "3.0.50106.0", false },
    { "4", "5", "", false }
};
static const VersionRangeDefinition kFlip4MacVersionRange[] = {
    { "", "", "2.2.1", false }
};
static const VersionRangeDefinition kShockwaveVersionRange[] = {
    { "",  "", "11.5.9.615", true }
};
// TODO(cevans) - I don't see Adobe Reader in here for Mac.
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", PluginGroup::kQuickTimeGroupName, "QuickTime Plug-in",
    kQuicktimeVersionRange, arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", PluginGroup::kJavaGroupName, "Java",
    kJavaVersionRange, arraysize(kJavaVersionRange),
    "http://support.apple.com/kb/HT1338" },
  { "silverlight", "Silverlight", "Silverlight", kSilverlightVersionRange,
    arraysize(kSilverlightVersionRange),
    "http://www.microsoft.com/getsilverlight/" },
  { "flip4mac", "Flip4Mac", "Flip4Mac", kFlip4MacVersionRange,
    arraysize(kFlip4MacVersionRange),
    "http://www.telestream.net/flip4mac-wmv/overview.htm" },
  { "shockwave", PluginGroup::kShockwaveGroupName, "Shockwave for Director",
    kShockwaveVersionRange, arraysize(kShockwaveVersionRange),
    "http://www.adobe.com/shockwave/download/" }
};

#elif defined(OS_WIN)
// TODO(panayiotis): We should group "RealJukebox NS Plugin" with the rest of
// the RealPlayer files.
static const VersionRangeDefinition kQuicktimeVersionRange[] = {
    { "", "", "7.6.8", true }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "7", "6.0.220", true }  // "220" is not a typo.
};
static const VersionRangeDefinition kAdobeReaderVersionRange[] = {
    { "10", "11", "", false },
    { "9", "10", "9.4.1", false },
    { "0", "9", "8.2.5", false }
};
static const VersionRangeDefinition kSilverlightVersionRange[] = {
    { "0", "4", "3.0.50106.0", false },
    { "4", "5", "", false }
};
static const VersionRangeDefinition kShockwaveVersionRange[] = {
    { "", "", "11.5.9.615", true }
};
static const VersionRangeDefinition kDivXVersionRange[] = {
    { "", "", "1.4.3.4", false }
};
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", PluginGroup::kQuickTimeGroupName, "QuickTime Plug-in",
    kQuicktimeVersionRange, arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", PluginGroup::kJavaGroupName, "Java",
    kJavaVersionRange, arraysize(kJavaVersionRange), "http://www.java.com/" },
  { "adobe-reader", PluginGroup::kAdobeReaderGroupName, "Adobe Acrobat",
    kAdobeReaderVersionRange, arraysize(kAdobeReaderVersionRange),
    "http://get.adobe.com/reader/" },
  { "silverlight", "Silverlight", "Silverlight", kSilverlightVersionRange,
    arraysize(kSilverlightVersionRange),
    "http://www.microsoft.com/getsilverlight/" },
  { "shockwave", PluginGroup::kShockwaveGroupName, "Shockwave for Director",
    kShockwaveVersionRange, arraysize(kShockwaveVersionRange),
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
    { "0", "1.7", "1.6.0.22", true }
};

static const VersionRangeDefinition kRedhatIcedTeaVersionRange[] = {
    { "0", "1.9", "1.8.5", true },
    { "1.9", "1.10", "1.9.5", true },
};

static const PluginGroupDefinition kGroupDefinitions[] = {
  // Flash on Linux is significant because there isn't yet a built-in Flash
  // plug-in on the Linux 64-bit version of Chrome.
  kFlashDefinition,
  { "java-runtime-environment", PluginGroup::kJavaGroupName, "Java",
    kJavaVersionRange, arraysize(kJavaVersionRange),
    "http://www.java.com/en/download/manual.jsp" },
  { "redhat-icetea-java", "IcedTea", "IcedTea",
    kRedhatIcedTeaVersionRange, arraysize(kRedhatIcedTeaVersionRange),
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
  base::AutoLock lock(lock_);
  return plugins_loaded_;
}

void PluginList::RefreshPlugins() {
  base::AutoLock lock(lock_);
  plugins_need_refresh_ = true;
}

void PluginList::AddExtraPluginPath(const FilePath& plugin_path) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  base::AutoLock lock(lock_);
  extra_plugin_paths_.push_back(plugin_path);
#endif
}

void PluginList::RemoveExtraPluginPath(const FilePath& plugin_path) {
  base::AutoLock lock(lock_);
  std::vector<FilePath>::iterator it =
      std::find(extra_plugin_paths_.begin(), extra_plugin_paths_.end(),
                plugin_path);
  if (it != extra_plugin_paths_.end())
    extra_plugin_paths_.erase(it);
}

void PluginList::AddExtraPluginDir(const FilePath& plugin_dir) {
  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  base::AutoLock lock(lock_);
  extra_plugin_dirs_.push_back(plugin_dir);
#endif
}

void PluginList::RegisterInternalPlugin(const WebPluginInfo& info) {
  PluginEntryPoints entry_points = {0};
  InternalPlugin plugin = { info, entry_points };

  base::AutoLock lock(lock_);
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
  plugin.info.enabled = WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED;

  WebPluginMimeType mime_type;
  mime_type.mime_type = mime_type_str;
  plugin.info.mime_types.push_back(mime_type);

  plugin.entry_points = entry_points;

  base::AutoLock lock(lock_);
  internal_plugins_.push_back(plugin);
}

void PluginList::UnregisterInternalPlugin(const FilePath& path) {
  base::AutoLock lock(lock_);
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
    base::AutoLock lock(lock_);
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
      disable_outdated_plugins_(false) {
  PlatformInit();
  AddHardcodedPluginGroups(&plugin_groups_);
}

void PluginList::LoadPluginsInternal(ScopedVector<PluginGroup>* plugin_groups) {
  // Don't want to hold the lock while loading new plugins, so we don't block
  // other methods if they're called on other threads.
  std::vector<FilePath> extra_plugin_paths;
  std::vector<FilePath> extra_plugin_dirs;
  std::vector<InternalPlugin> internal_plugins;
  {
    base::AutoLock lock(lock_);
    // Clear the refresh bit now, because it might get set again before we
    // reach the end of the method.
    plugins_need_refresh_ = false;
    extra_plugin_paths = extra_plugin_paths_;
    extra_plugin_dirs = extra_plugin_dirs_;
    internal_plugins = internal_plugins_;
  }

  std::set<FilePath> visited_plugins;

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  // Load internal plugins first so that, if both an internal plugin and a
  // "discovered" plugin want to handle the same type, the internal plugin
  // will have precedence.
  for (size_t i = 0; i < internal_plugins.size(); ++i) {
    if (internal_plugins[i].info.path.value() == kDefaultPluginLibraryName)
      continue;
    LoadPlugin(internal_plugins[i].info.path, plugin_groups);
  }

  for (size_t i = 0; i < extra_plugin_paths.size(); ++i) {
    const FilePath& path = extra_plugin_paths[i];
    if (visited_plugins.find(path) != visited_plugins.end())
      continue;
    LoadPlugin(path, plugin_groups);
    visited_plugins.insert(path);
  }

  for (size_t i = 0; i < extra_plugin_dirs.size(); ++i) {
    LoadPluginsFromDir(
        extra_plugin_dirs[i], plugin_groups, &visited_plugins);
  }

  for (size_t i = 0; i < directories_to_scan.size(); ++i) {
    LoadPluginsFromDir(
        directories_to_scan[i], plugin_groups, &visited_plugins);
  }

#if defined(OS_WIN)
  LoadPluginsFromRegistry(plugin_groups, &visited_plugins);
#endif

  // Load the default plugin last.
  if (webkit_glue::IsDefaultPluginEnabled())
    LoadPlugin(FilePath(kDefaultPluginLibraryName), plugin_groups);
}

void PluginList::LoadPlugins(bool refresh) {
  {
    base::AutoLock lock(lock_);
    if (plugins_loaded_ && !refresh && !plugins_need_refresh_)
      return;
  }

  ScopedVector<PluginGroup> new_plugin_groups;
  AddHardcodedPluginGroups(&new_plugin_groups);
  // Do the actual loading of the plugins.
  LoadPluginsInternal(&new_plugin_groups);

  base::AutoLock lock(lock_);
  // Grab all plugins that were found before to copy enabled statuses.
  std::vector<WebPluginInfo> old_plugins;
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& gr_plugins =
        plugin_groups_[i]->web_plugins_info();
    old_plugins.insert(old_plugins.end(), gr_plugins.begin(), gr_plugins.end());
  }
  // Disable all of the plugins and plugin groups that are disabled by policy.
  for (size_t i = 0; i < new_plugin_groups.size(); ++i) {
    PluginGroup* group = new_plugin_groups[i];
    string16 group_name = group->GetGroupName();

    std::vector<WebPluginInfo>& gr_plugins = group->GetPluginsContainer();
    for (size_t j = 0; j < gr_plugins.size(); ++j) {
      int plugin_found = -1;
      for (size_t k = 0; k < old_plugins.size(); ++k) {
        if (gr_plugins[j].path == old_plugins[k].path) {
          plugin_found = k;
          break;
        }
      }
      if (plugin_found >= 0)
        gr_plugins[j].enabled = old_plugins[plugin_found].enabled;
      // Set the disabled flag of all plugins scheduled for disabling.
      if (plugins_to_disable_.find(gr_plugins[j].path) !=
          plugins_to_disable_.end()) {
          group->DisablePlugin(gr_plugins[j].path);
      }
    }
    if (group->IsEmpty()) {
      if (!group->Enabled())
        groups_to_disable_.insert(group->GetGroupName());
      new_plugin_groups.erase(new_plugin_groups.begin() + i);
      --i;
      continue;
    }

    group->EnforceGroupPolicy();
    if (disable_outdated_plugins_)
      group->DisableOutdatedPlugins();
  }
  // We flush the list of prematurely disabled plugins after the load has
  // finished. If for some reason a plugin reappears on a second load it is
  // going to be loaded normally. This is only true for non-policy controlled
  // plugins though.
  plugins_to_disable_.clear();

  plugin_groups_.swap(new_plugin_groups);
  plugins_loaded_ = true;
}

void PluginList::LoadPlugin(const FilePath& path,
                            ScopedVector<PluginGroup>* plugin_groups) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Loading plugin " << path.value();
  WebPluginInfo plugin_info;
  const PluginEntryPoints* entry_points;

  if (!ReadPluginInfo(path, &plugin_info, &entry_points))
    return;

  if (!ShouldLoadPlugin(plugin_info, plugin_groups))
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

  AddToPluginGroups(plugin_info, plugin_groups);
}

void PluginList::GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  LoadPlugins(refresh);

  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& gr_plugins =
        plugin_groups_[i]->web_plugins_info();
    plugins->insert(plugins->end(), gr_plugins.begin(), gr_plugins.end());
  }
}

void PluginList::GetEnabledPlugins(bool refresh,
                                   std::vector<WebPluginInfo>* plugins) {
  LoadPlugins(refresh);

  plugins->clear();
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& gr_plugins =
        plugin_groups_[i]->web_plugins_info();
    for (size_t i = 0; i < gr_plugins.size(); ++i) {
      if (IsPluginEnabled(gr_plugins[i]))
        plugins->push_back(gr_plugins[i]);
    }
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
  base::AutoLock lock(lock_);
  info->clear();
  if (actual_mime_types)
    actual_mime_types->clear();

  std::set<FilePath> visited_plugins;

  // Add in enabled plugins by mime type.
  WebPluginInfo default_plugin;
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& plugins =
        plugin_groups_[i]->web_plugins_info();
    for (size_t i = 0; i < plugins.size(); ++i) {
      if (IsPluginEnabled(plugins[i]) && SupportsType(plugins[i],
                                             mime_type, allow_wildcard)) {
        FilePath path = plugins[i].path;
        if (path.value() != kDefaultPluginLibraryName &&
            visited_plugins.insert(path).second) {
          info->push_back(plugins[i]);
          if (actual_mime_types)
            actual_mime_types->push_back(mime_type);
        }
      }
    }
  }

  // Add in enabled plugins by url.
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot != std::string::npos) {
    std::string extension = StringToLowerASCII(std::string(path, last_dot+1));
    std::string actual_mime_type;
    for (size_t i = 0; i < plugin_groups_.size(); ++i) {
      const std::vector<WebPluginInfo>& plugins =
          plugin_groups_[i]->web_plugins_info();
      for (size_t i = 0; i < plugins.size(); ++i) {
        if (IsPluginEnabled(plugins[i]) &&
            SupportsExtension(plugins[i], extension, &actual_mime_type)) {
          FilePath path = plugins[i].path;
          if (path.value() != kDefaultPluginLibraryName &&
              visited_plugins.insert(path).second) {
            info->push_back(plugins[i]);
            if (actual_mime_types)
              actual_mime_types->push_back(actual_mime_type);
          }
        }
      }
    }
  }

  // Add in disabled plugins by mime type.
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& plugins =
        plugin_groups_[i]->web_plugins_info();
    for (size_t i = 0; i < plugins.size(); ++i) {
      if (!IsPluginEnabled(plugins[i]) &&
          SupportsType(plugins[i], mime_type, allow_wildcard)) {
        FilePath path = plugins[i].path;
        if (path.value() != kDefaultPluginLibraryName &&
            visited_plugins.insert(path).second) {
          info->push_back(plugins[i]);
          if (actual_mime_types)
            actual_mime_types->push_back(mime_type);
        }
      }
    }
  }

  // Add the default plugin at the end if it supports the mime type given,
  // and the default plugin is enabled.
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
#if defined(OS_WIN)
    if (plugin_groups_[i]->identifier().compare(
        WideToUTF8(kDefaultPluginLibraryName)) == 0) {
#else
    if (plugin_groups_[i]->identifier().compare(
        kDefaultPluginLibraryName) == 0) {
#endif
      DCHECK_NE(0U, plugin_groups_[i]->web_plugins_info().size());
      const WebPluginInfo& default_info =
          plugin_groups_[i]->web_plugins_info()[0];
      if (SupportsType(default_info, mime_type, allow_wildcard)) {
        info->push_back(default_info);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
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
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<WebPluginInfo>& plugins =
        plugin_groups_[i]->web_plugins_info();
    for (size_t i = 0; i < plugins.size(); ++i) {
      if (plugins[i].path == plugin_path) {
        *info = plugins[i];
        return true;
      }
    }
  }

  return false;
}

void PluginList::GetPluginGroups(
    bool load_if_necessary,
    std::vector<PluginGroup>* plugin_groups) {
  if (load_if_necessary)
    LoadPlugins(false);
  base::AutoLock lock(lock_);
  plugin_groups->clear();
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    // In some unit tests we can get confronted with empty groups but in real
    // world code this if should never be false here.
    if (!plugin_groups_[i]->IsEmpty())
      plugin_groups->push_back(*plugin_groups_[i]);
  }
}

const PluginGroup* PluginList::GetPluginGroup(
    const WebPluginInfo& web_plugin_info) {
  base::AutoLock lock(lock_);
  return AddToPluginGroups(web_plugin_info, &plugin_groups_);
}

string16 PluginList::GetPluginGroupName(const std::string& identifier) {
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    if (plugin_groups_[i]->identifier() == identifier)
      return plugin_groups_[i]->GetGroupName();
  }
  return string16();
}

std::string PluginList::GetPluginGroupIdentifier(
    const WebPluginInfo& web_plugin_info) {
  base::AutoLock lock(lock_);
  PluginGroup* group = AddToPluginGroups(web_plugin_info, &plugin_groups_);
  return group->identifier();
}

void PluginList::AddHardcodedPluginGroups(ScopedVector<PluginGroup>* groups) {
  base::AutoLock lock(lock_);
  const PluginGroupDefinition* definitions = GetPluginGroupDefinitions();
  size_t num_definitions = GetPluginGroupDefinitionsSize();
  for (size_t i = 0; i < num_definitions; ++i)
    groups->push_back(PluginGroup::FromPluginGroupDefinition(definitions[i]));
}

PluginGroup* PluginList::AddToPluginGroups(
    const WebPluginInfo& web_plugin_info,
    ScopedVector<PluginGroup>* plugin_groups) {
  PluginGroup* group = NULL;
  for (size_t i = 0; i < plugin_groups->size(); ++i) {
    if ((*plugin_groups)[i]->Match(web_plugin_info)) {
      group = (*plugin_groups)[i];
      break;
    }
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
    for (size_t i = 0; i < plugin_groups->size(); ++i) {
      if ((*plugin_groups)[i]->identifier() == identifier) {
        group->set_identifier(PluginGroup::GetLongIdentifier(web_plugin_info));
        break;
      }
    }
    plugin_groups->push_back(group);
  }
  group->AddPlugin(web_plugin_info);
  // If group is scheduled for disabling do that now and remove it from the
  // list.
  if (groups_to_disable_.erase(group->GetGroupName()))
    group->EnableGroup(false);
  return group;
}

bool PluginList::EnablePlugin(const FilePath& filename) {
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    if (plugin_groups_[i]->ContainsPlugin(filename))
      return plugin_groups_[i]->EnablePlugin(filename);
  }
  // Non existing plugin is being enabled. Check if it has been disabled before
  // and remove it.
  return (plugins_to_disable_.erase(filename) != 0);
}

bool PluginList::DisablePlugin(const FilePath& filename) {
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    if (plugin_groups_[i]->ContainsPlugin(filename))
      return plugin_groups_[i]->DisablePlugin(filename);
  }
  // Non existing plugin is being disabled. Queue the plugin so that on the next
  // load plugins call they will be disabled.
  plugins_to_disable_.insert(filename);
  return true;
}

bool PluginList::EnableGroup(bool enable, const string16& group_name) {
  base::AutoLock lock(lock_);
  PluginGroup* group = NULL;
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    if (plugin_groups_[i]->GetGroupName().find(group_name) != string16::npos) {
      group = plugin_groups_[i];
      break;
    }
  }
  if (!group) {
    // Non existing group is being enabled. Queue the group so that on the next
    // load plugins call they will be disabled.
    if (!enable) {
      groups_to_disable_.insert(group_name);
      return true;
    } else {
      return (groups_to_disable_.erase(group_name) != 0);
    }
  }

  return group->EnableGroup(enable);
}

bool PluginList::SupportsType(const WebPluginInfo& plugin,
                              const std::string& mime_type,
                              bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = plugin.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && mime_info.mime_type == "*")
        continue;
      return true;
    }
  }
  return false;
}

bool PluginList::SupportsExtension(const WebPluginInfo& plugin,
                                   const std::string& extension,
                                   std::string* actual_mime_type) {
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_type = plugin.mime_types[i];
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

void PluginList::DisableOutdatedPluginGroups() {
  disable_outdated_plugins_ = true;
}

PluginList::~PluginList() {
}


}  // namespace npapi
}  // namespace webkit
