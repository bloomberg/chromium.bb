// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace {

static const char kApplicationOctetStream[] = "application/octet-stream";

base::LazyInstance<webkit::npapi::PluginList> g_singleton =
    LAZY_INSTANCE_INITIALIZER;

bool AllowMimeTypeMismatch(const std::string& orig_mime_type,
                           const std::string& actual_mime_type) {
  if (orig_mime_type == actual_mime_type) {
    NOTREACHED();
    return true;
  }

  // We do not permit URL-sniff based plug-in MIME type overrides aside from
  // the case where the "type" was initially missing or generic
  // (application/octet-stream).
  // We collected stats to determine this approach isn't a major compat issue,
  // and we defend against content confusion attacks in various cases, such
  // as when the user doesn't have the Flash plug-in enabled.
  bool allow = orig_mime_type.empty() ||
               orig_mime_type == kApplicationOctetStream;
  LOG_IF(INFO, !allow) << "Ignoring plugin with unexpected MIME type "
                       << actual_mime_type << " (expected " << orig_mime_type
                       << ")";
  return allow;
}

}

namespace webkit {
namespace npapi {

FilePath::CharType kDefaultPluginLibraryName[] =
    FILE_PATH_LITERAL("default_plugin");

// Some version ranges can be shared across operating systems. This should be
// done where possible to avoid duplication.
// This is up to date with
// http://www.adobe.com/support/security/bulletins/apsb11-26.html
static const VersionRangeDefinition kFlashVersionRange[] = {
    { "", "", "10.3.183", false }
};
// This is up to date with
// http://www.adobe.com/support/security/bulletins/apsb11-19.html
static const VersionRangeDefinition kShockwaveVersionRange[] = {
    { "",  "", "11.6.1.629", true }
};
static const VersionRangeDefinition kSilverlightVersionRange[] = {
    { "0", "4", "3.0.50611.0", false },
    { "4", "5", "", false }
};

// Similarly, try and share the group definition for plug-ins that are
// very consistent across OS'es.
#define kFlashDefinition { \
    "adobe-flash-player", "Flash", "Shockwave Flash", kFlashVersionRange,\
    arraysize(kFlashVersionRange), "http://get.adobe.com/flashplayer/" }

#define kShockwaveDefinition { \
    "shockwave", PluginGroup::kShockwaveGroupName, "Shockwave for Director", \
    kShockwaveVersionRange, arraysize(kShockwaveVersionRange), \
    "http://www.adobe.com/shockwave/download/" }

#define kSilverlightDefinition { \
    "silverlight", PluginGroup::kSilverlightGroupName, "Silverlight", \
    kSilverlightVersionRange, arraysize(kSilverlightVersionRange), \
    "http://www.microsoft.com/getsilverlight/" }

#if defined(OS_MACOSX)
// Plugin Groups for Mac.
// Plugins are listed here as soon as vulnerabilities and solutions
// (new versions) are published.
static const VersionRangeDefinition kQuicktimeVersionRange[] = {
    { "", "", "7.6.6", true }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "13.0", "12.8.0", true },  // Leopard
    { "13.0", "14.0", "13.5.0", true },  // Snow Leopard
    { "14.0", "", "14.0.3", true }  // Lion
};
static const VersionRangeDefinition kFlip4MacVersionRange[] = {
    { "", "", "2.2.1", true }
};
// Note: The Adobe Reader browser plug-in is not supported in Chrome.
// Note: The Real Player plugin for mac doesn't expose a version at all.
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", PluginGroup::kQuickTimeGroupName, "QuickTime Plug-in",
    kQuicktimeVersionRange, arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", PluginGroup::kJavaGroupName, "Java",
    kJavaVersionRange, arraysize(kJavaVersionRange),
    "http://support.apple.com/kb/HT1338" },
  kSilverlightDefinition,
  { "flip4mac", "Flip4Mac", "Flip4Mac", kFlip4MacVersionRange,
    arraysize(kFlip4MacVersionRange),
    "http://www.telestream.net/flip4mac-wmv/overview.htm" },
  kShockwaveDefinition
};

#elif defined(OS_WIN)
// TODO(panayiotis): We should group "RealJukebox NS Plugin" with the rest of
// the RealPlayer files.
static const VersionRangeDefinition kQuicktimeVersionRange[] = {
    { "", "", "7.6.9", true }
};
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "7", "6.0.290", true },  // "290" is not a typo.
    { "7", "", "10.1", true }  // JDK7u1 identifies itself as 10.1
};
// This is up to date with
// http://www.adobe.com/support/security/bulletins/apsb11-24.html
static const VersionRangeDefinition kAdobeReaderVersionRange[] = {
    { "10", "11", "10.1.1", false },
    { "9", "10", "9.4.6", false },
    { "0", "9", "8.3.1", false }
};
static const VersionRangeDefinition kDivXVersionRange[] = {
    { "", "", "1.4.3.4", false }
};
static const VersionRangeDefinition kRealPlayerVersionRange[] = {
    { "", "", "12.0.1.666", true }
};
static const VersionRangeDefinition kWindowsMediaPlayerVersionRange[] = {
    { "", "", "", true }
};
static const PluginGroupDefinition kGroupDefinitions[] = {
  kFlashDefinition,
  { "apple-quicktime", PluginGroup::kQuickTimeGroupName, "QuickTime Plug-in",
    kQuicktimeVersionRange, arraysize(kQuicktimeVersionRange),
    "http://www.apple.com/quicktime/download/" },
  { "java-runtime-environment", PluginGroup::kJavaGroupName, "Java",
    kJavaVersionRange, arraysize(kJavaVersionRange),
    "http://www.java.com/download" },
  { "adobe-reader", PluginGroup::kAdobeReaderGroupName, "Adobe Acrobat",
    kAdobeReaderVersionRange, arraysize(kAdobeReaderVersionRange),
    "http://get.adobe.com/reader/" },
  kSilverlightDefinition,
  kShockwaveDefinition,
  { "divx-player", "DivX Player", "DivX Web Player", kDivXVersionRange,
    arraysize(kDivXVersionRange),
    "http://download.divx.com/divx/autoupdate/player/"
    "DivXWebPlayerInstaller.exe" },
  { "realplayer", PluginGroup::kRealPlayerGroupName, "RealPlayer",
    kRealPlayerVersionRange, arraysize(kRealPlayerVersionRange),
    "http://www.real.com/realplayer/download" },
  // These are here for grouping, no vulnerabilities known.
  { "windows-media-player", PluginGroup::kWindowsMediaPlayerGroupName,
    "Windows Media Player", kWindowsMediaPlayerVersionRange,
    arraysize(kWindowsMediaPlayerVersionRange), "" },
  { "microsoft-office", "Microsoft Office", "Microsoft Office",
    NULL, 0, "" },
};

#elif defined(OS_CHROMEOS)
// ChromeOS generally has (autoupdated) system plug-ins and no user-installable
// plug-ins.
static const PluginGroupDefinition kGroupDefinitions[] = { };

#else  // Most importantly, covers desktop Linux.
static const VersionRangeDefinition kJavaVersionRange[] = {
    { "0", "1.7", "1.6.0.29", true },
    { "1.7", "", "1.7.0.1", true }
};

static const VersionRangeDefinition kRedhatIcedTeaVersionRange[] = {
    { "0", "1.9", "1.8.10", true },
    { "1.9", "1.10", "1.9.10", true },
    { "1.10", "", "1.10.4", true }
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
PluginList* PluginList::Singleton() {
  return g_singleton.Pointer();
}

// static
bool PluginList::DebugPluginLoading() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDebugPluginLoading);
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

void PluginList::RegisterInternalPlugin(const webkit::WebPluginInfo& info) {
  PluginEntryPoints entry_points = {0};
  InternalPlugin plugin = { info, entry_points };

  base::AutoLock lock(lock_);
  // Newer registrations go earlier in the list so they can override the MIME
  // types of older registrations.
  internal_plugins_.insert(internal_plugins_.begin(), plugin);

  if (info.path.value() == kDefaultPluginLibraryName)
    default_plugin_enabled_ = true;
}

void PluginList::RegisterInternalPlugin(const webkit::WebPluginInfo& info,
                                        const PluginEntryPoints& entry_points,
                                        bool add_at_beginning) {
  InternalPlugin plugin = { info, entry_points };

  base::AutoLock lock(lock_);

  if (add_at_beginning) {
    // Newer registrations go earlier in the list so they can override the MIME
    // types of older registrations.
    internal_plugins_.insert(internal_plugins_.begin(), plugin);
  } else {
    internal_plugins_.push_back(plugin);
  }

  if (info.path.value() == kDefaultPluginLibraryName)
    default_plugin_enabled_ = true;
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

void PluginList::GetInternalPlugins(
    std::vector<webkit::WebPluginInfo>* internal_plugins) {
  base::AutoLock lock(lock_);

  for (std::vector<InternalPlugin>::iterator it = internal_plugins_.begin();
       it != internal_plugins_.end();
       ++it) {
    internal_plugins->push_back(it->info);
  }
}

bool PluginList::ReadPluginInfo(const FilePath& filename,
                                webkit::WebPluginInfo* info,
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
    std::vector<webkit::WebPluginMimeType>* parsed_mime_types) {
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
    : plugins_need_refresh_(true),
      default_plugin_enabled_(false) {
  PlatformInit();
  AddHardcodedPluginGroups(kGroupDefinitions,
                           ARRAYSIZE_UNSAFE(kGroupDefinitions));
}

PluginList::PluginList(const PluginGroupDefinition* definitions,
                       size_t num_definitions)
    : plugins_need_refresh_(true),
      default_plugin_enabled_(false) {
  // Don't do platform-dependend initialization in unit tests.
  AddHardcodedPluginGroups(definitions, num_definitions);
}

PluginGroup* PluginList::CreatePluginGroup(
      const webkit::WebPluginInfo& web_plugin_info) const {
  for (size_t i = 0; i < hardcoded_plugin_groups_.size(); ++i) {
    const PluginGroup* group = hardcoded_plugin_groups_[i];
    if (group->Match(web_plugin_info))
      return new PluginGroup(*group);
  }
  return PluginGroup::FromWebPluginInfo(web_plugin_info);
}

void PluginList::LoadPluginsInternal(ScopedVector<PluginGroup>* plugin_groups) {
  base::Closure will_load_callback;
  {
    base::AutoLock lock(lock_);
    // Clear the refresh bit now, because it might get set again before we
    // reach the end of the method.
    plugins_need_refresh_ = false;
    will_load_callback = will_load_plugins_callback_;
  }

  if (!will_load_callback.is_null())
    will_load_callback.Run();

  std::vector<FilePath> plugin_paths;
  GetPluginPathsToLoad(&plugin_paths);

  for (std::vector<FilePath>::const_iterator it = plugin_paths.begin();
       it != plugin_paths.end();
       ++it) {
    LoadPlugin(*it, plugin_groups);
  }
}

void PluginList::LoadPlugins() {
  {
    base::AutoLock lock(lock_);
    if (!plugins_need_refresh_)
      return;
  }

  ScopedVector<PluginGroup> new_plugin_groups;
  // Do the actual loading of the plugins.
  LoadPluginsInternal(&new_plugin_groups);

  base::AutoLock lock(lock_);
  plugin_groups_.swap(new_plugin_groups);
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
      if (mime_type == "*")
        return;
    }
  }

  base::AutoLock lock(lock_);
  AddToPluginGroups(plugin_info, plugin_groups);
}

void PluginList::GetPluginPathsToLoad(std::vector<FilePath>* plugin_paths) {
  // Don't want to hold the lock while loading new plugins, so we don't block
  // other methods if they're called on other threads.
  std::vector<FilePath> extra_plugin_paths;
  std::vector<FilePath> extra_plugin_dirs;
  std::vector<InternalPlugin> internal_plugins;
  {
    base::AutoLock lock(lock_);
    extra_plugin_paths = extra_plugin_paths_;
    extra_plugin_dirs = extra_plugin_dirs_;
    internal_plugins = internal_plugins_;
  }

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  // Load internal plugins first so that, if both an internal plugin and a
  // "discovered" plugin want to handle the same type, the internal plugin
  // will have precedence.
  for (size_t i = 0; i < internal_plugins.size(); ++i) {
    if (internal_plugins[i].info.path.value() == kDefaultPluginLibraryName)
      continue;
    plugin_paths->push_back(internal_plugins[i].info.path);
  }

  for (size_t i = 0; i < extra_plugin_paths.size(); ++i) {
    const FilePath& path = extra_plugin_paths[i];
    if (std::find(plugin_paths->begin(), plugin_paths->end(), path) !=
        plugin_paths->end()) {
      continue;
    }
    plugin_paths->push_back(path);
  }

  for (size_t i = 0; i < extra_plugin_dirs.size(); ++i)
    GetPluginsInDir(extra_plugin_dirs[i], plugin_paths);

  for (size_t i = 0; i < directories_to_scan.size(); ++i)
    GetPluginsInDir(directories_to_scan[i], plugin_paths);

#if defined(OS_WIN)
  GetPluginPathsFromRegistry(plugin_paths);
#endif

  // Load the default plugin last.
  if (default_plugin_enabled_)
    plugin_paths->push_back(FilePath(kDefaultPluginLibraryName));
}

void PluginList::SetPlugins(const std::vector<webkit::WebPluginInfo>& plugins) {
  base::AutoLock lock(lock_);

  plugins_need_refresh_ = false;

  plugin_groups_.reset();
  for (std::vector<webkit::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    AddToPluginGroups(*it, &plugin_groups_);
  }
}

void PluginList::set_will_load_plugins_callback(const base::Closure& callback) {
  base::AutoLock lock(lock_);
  will_load_plugins_callback_ = callback;
}

void PluginList::GetPlugins(std::vector<WebPluginInfo>* plugins) {
  LoadPlugins();
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<webkit::WebPluginInfo>& gr_plugins =
        plugin_groups_[i]->web_plugin_infos();
    plugins->insert(plugins->end(), gr_plugins.begin(), gr_plugins.end());
  }
}

bool PluginList::GetPluginsIfNoRefreshNeeded(
    std::vector<webkit::WebPluginInfo>* plugins) {
  base::AutoLock lock(lock_);
  if (plugins_need_refresh_)
    return false;

  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<webkit::WebPluginInfo>& gr_plugins =
        plugin_groups_[i]->web_plugin_infos();
    plugins->insert(plugins->end(), gr_plugins.begin(), gr_plugins.end());
  }
  return true;
}

void PluginList::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    bool* use_stale,
    std::vector<webkit::WebPluginInfo>* info,
    std::vector<std::string>* actual_mime_types) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));
  DCHECK(info);

  if (!use_stale)
    LoadPlugins();
  base::AutoLock lock(lock_);
  if (use_stale)
    *use_stale = plugins_need_refresh_;
  info->clear();
  if (actual_mime_types)
    actual_mime_types->clear();

  std::set<FilePath> visited_plugins;

  // Add in plugins by mime type.
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    const std::vector<webkit::WebPluginInfo>& plugins =
        plugin_groups_[i]->web_plugin_infos();
    for (size_t i = 0; i < plugins.size(); ++i) {
      if (SupportsType(plugins[i], mime_type, allow_wildcard)) {
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

  // Add in plugins by url.
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot != std::string::npos) {
    std::string extension = StringToLowerASCII(std::string(path, last_dot+1));
    std::string actual_mime_type;
    for (size_t i = 0; i < plugin_groups_.size(); ++i) {
      const std::vector<webkit::WebPluginInfo>& plugins =
          plugin_groups_[i]->web_plugin_infos();
      for (size_t i = 0; i < plugins.size(); ++i) {
        if (SupportsExtension(plugins[i], extension, &actual_mime_type)) {
          FilePath path = plugins[i].path;
          if (path.value() != kDefaultPluginLibraryName &&
              visited_plugins.insert(path).second &&
              AllowMimeTypeMismatch(mime_type, actual_mime_type)) {
            info->push_back(plugins[i]);
            if (actual_mime_types)
              actual_mime_types->push_back(actual_mime_type);
          }
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
      DCHECK_NE(0U, plugin_groups_[i]->web_plugin_infos().size());
      const webkit::WebPluginInfo& default_info =
          plugin_groups_[i]->web_plugin_infos()[0];
      if (SupportsType(default_info, mime_type, allow_wildcard)) {
        info->push_back(default_info);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
    }
  }
}

void PluginList::GetPluginGroups(
    bool load_if_necessary,
    std::vector<PluginGroup>* plugin_groups) {
  if (load_if_necessary)
    LoadPlugins();
  base::AutoLock lock(lock_);
  plugin_groups->clear();
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    // In some unit tests we can get confronted with empty groups but in real
    // world code this if should never be false here.
    if (!plugin_groups_[i]->IsEmpty())
      plugin_groups->push_back(*plugin_groups_[i]);
  }
}

PluginGroup* PluginList::GetPluginGroup(
    const webkit::WebPluginInfo& web_plugin_info) {
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < plugin_groups_->size(); ++i) {
    const std::vector<webkit::WebPluginInfo>& plugins =
        plugin_groups_[i]->web_plugin_infos();
    for (size_t j = 0; j < plugins.size(); ++j) {
      if (plugins[j].path == web_plugin_info.path) {
        return new PluginGroup(*plugin_groups_[i]);
      }
    }
  }
  PluginGroup* group = CreatePluginGroup(web_plugin_info);
  group->AddPlugin(web_plugin_info);
  return group;
}

string16 PluginList::GetPluginGroupName(const std::string& identifier) {
  for (size_t i = 0; i < plugin_groups_.size(); ++i) {
    if (plugin_groups_[i]->identifier() == identifier)
      return plugin_groups_[i]->GetGroupName();
  }
  return string16();
}

void PluginList::AddHardcodedPluginGroups(
    const PluginGroupDefinition* group_definitions,
    size_t num_group_definitions) {
  for (size_t i = 0; i < num_group_definitions; ++i) {
    hardcoded_plugin_groups_->push_back(
        PluginGroup::FromPluginGroupDefinition(group_definitions[i]));
  }
}

PluginGroup* PluginList::AddToPluginGroups(
    const webkit::WebPluginInfo& web_plugin_info,
    ScopedVector<PluginGroup>* plugin_groups) {
  PluginGroup* group = NULL;
  for (size_t i = 0; i < plugin_groups->size(); ++i) {
    if ((*plugin_groups)[i]->Match(web_plugin_info)) {
      group = (*plugin_groups)[i];
      break;
    }
  }
  if (!group) {
    group = CreatePluginGroup(web_plugin_info);
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
  return group;
}

bool PluginList::SupportsType(const webkit::WebPluginInfo& plugin,
                              const std::string& mime_type,
                              bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const webkit::WebPluginMimeType& mime_info = plugin.mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && mime_info.mime_type == "*")
        continue;
      return true;
    }
  }
  return false;
}

bool PluginList::SupportsExtension(const webkit::WebPluginInfo& plugin,
                                   const std::string& extension,
                                   std::string* actual_mime_type) {
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    const webkit::WebPluginMimeType& mime_type = plugin.mime_types[i];
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

PluginList::~PluginList() {
}


}  // namespace npapi
}  // namespace webkit
