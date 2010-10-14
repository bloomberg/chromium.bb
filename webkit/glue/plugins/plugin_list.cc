// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

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
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_switches.h"
#include "webkit/glue/webkit_glue.h"

namespace NPAPI {

base::LazyInstance<PluginList> g_singleton(base::LINKER_INITIALIZED);

static LoadPluginsFromDiskHookFunc g_load_plugins_hook;

// static
PluginList* PluginList::Singleton() {
  return g_singleton.Pointer();
}

// static
void PluginList::SetPluginLoadHook(LoadPluginsFromDiskHookFunc hook) {
  g_load_plugins_hook = hook;
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

void PluginList::RegisterInternalPlugin(const PluginVersionInfo& info) {
  AutoLock lock(lock_);
  internal_plugins_.push_back(info);
}

void PluginList::UnregisterInternalPlugin(const FilePath& path) {
  AutoLock lock(lock_);
  for (size_t i = 0; i < internal_plugins_.size(); i++) {
    if (internal_plugins_[i].path == path) {
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
      if (filename == internal_plugins_[i].path) {
        *entry_points = &internal_plugins_[i].entry_points;
        return CreateWebPluginInfo(internal_plugins_[i], info);
      }
    }
  }

  // Not an internal plugin.
  *entry_points = NULL;

  return PluginLib::ReadWebPluginInfo(filename, info);
}

bool PluginList::CreateWebPluginInfo(const PluginVersionInfo& pvi,
                                     WebPluginInfo* info) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<string16> descriptions;
  base::SplitString(WideToUTF8(pvi.mime_types), '|', &mime_types);
  base::SplitString(WideToUTF8(pvi.file_extensions), '|', &file_extensions);
  base::SplitString(WideToUTF16(pvi.type_descriptions), '|', &descriptions);

  info->mime_types.clear();

  if (mime_types.empty()) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << pvi.product_name << " has no MIME types, skipping";
    return false;
  }

  info->name = WideToUTF16(pvi.product_name);
  info->desc = WideToUTF16(pvi.file_description);
  info->version = WideToUTF16(pvi.file_version);
  info->path = pvi.path;
  info->enabled = true;

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
        if (ext > 1 && mime_type.description[ext -1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    info->mime_types.push_back(mime_type);
  }

  return true;
}

PluginList::PluginList()
    : plugins_loaded_(false),
      plugins_need_refresh_(false),
      disable_outdated_plugins_(false) {
  PlatformInit();
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
  std::vector<PluginVersionInfo> internal_plugins;
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

  if (g_load_plugins_hook)
    g_load_plugins_hook();

  std::vector<WebPluginInfo> new_plugins;
  std::set<FilePath> visited_plugins;

  std::vector<FilePath> directories_to_scan;
  GetPluginDirectories(&directories_to_scan);

  // Load internal plugins first so that, if both an internal plugin and a
  // "discovered" plugin want to handle the same type, the internal plugin
  // will have precedence.
  for (size_t i = 0; i < internal_plugins.size(); ++i) {
    if (internal_plugins[i].path.value() == kDefaultPluginLibraryName)
      continue;
    LoadPlugin(internal_plugins[i].path, &new_plugins);
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

#if defined OS_WIN
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
  PluginMap plugin_groups;
  GetPluginGroups(&new_plugins, &plugin_groups);
  for (PluginMap::const_iterator it = plugin_groups.begin();
       it != plugin_groups.end(); ++it) {
    PluginGroup* group = it->second.get();
    string16 group_name = group->GetGroupName();
    if (ShouldDisableGroup(group_name)) {
      it->second->Enable(false);
    }

    if (disable_outdated_plugins_) {
      group->DisableOutdatedPlugins();
      if (!group->Enabled()) {
        AutoLock lock(lock_);
        disabled_groups_.insert(group_name);
      }
    }
  }

  // Only update the data now since loading plugins can take a while.
  AutoLock lock(lock_);

  // Mark disabled plugins as such.
  for (size_t i = 0; i < new_plugins.size(); ++i) {
    if (disabled_plugins_.count(new_plugins[i].path))
      new_plugins[i].enabled = false;
  }

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

  plugins->push_back(plugin_info);
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

void PluginList::GetPluginInfoArray(const GURL& url,
                                    const std::string& mime_type,
                                    bool allow_wildcard,
                                    std::vector<WebPluginInfo>* info,
                                    std::vector<std::string>* actual_mime_types)
{
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

void PluginList::GetPluginGroups(bool load_if_necessary,
                                 PluginMap* plugin_groups) {
  if (load_if_necessary)
    LoadPlugins(false);

  AutoLock lock(lock_);
  GetPluginGroups(&plugins_, plugin_groups);
}

// static
void PluginList::GetPluginGroups(const std::vector<WebPluginInfo>* plugins,
                                 PluginMap* plugin_groups) {
  plugin_groups->clear();
  // We first search for an existing group that matches our name,
  // and only create a new group if we can't find any.
  for (size_t i = 0; i < plugins->size(); ++i) {
    const WebPluginInfo& web_plugin = (*plugins)[i];
    PluginGroup* group = PluginGroup::FindGroupMatchingPlugin(
        *plugin_groups, web_plugin);
    if (!group) {
      group = PluginGroup::CopyOrCreatePluginGroup(web_plugin);
      std::string identifier = group->identifier();
      // If the identifier is not unique, use the full path. This means that we
      // probably won't be able to search for this group by identifier, but at
      // least it's going to be in the set of plugin groups, and if there
      // is already a plug-in with the same filename, it's probably going to
      // handle the same MIME types (and it has a higher priority), so this one
      // is not going to run anyway.
      if (plugin_groups->find(identifier) != plugin_groups->end())
#if defined(OS_POSIX)
        identifier = web_plugin.path.value();
#elif defined(OS_WIN)
        identifier = base::SysWideToUTF8(web_plugin.path.value());
#endif
      DCHECK(plugin_groups->find(identifier) == plugin_groups->end());
      (*plugin_groups)[identifier] = linked_ptr<PluginGroup>(group);
    }
    group->AddPlugin(web_plugin, i);
  }
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

  PluginMap plugin_groups;
  GetPluginGroups(false, &plugin_groups);
  for (PluginMap::const_iterator it = plugin_groups.begin();
       it != plugin_groups.end(); ++it) {
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
}

void PluginList::Shutdown() {
  // TODO
}

}  // namespace NPAPI
