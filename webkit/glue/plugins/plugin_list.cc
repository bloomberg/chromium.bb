// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/mime_util.h"
#include "webkit/default_plugin/plugin_main.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"
#include "googleurl/src/gurl.h"

#if defined(USE_LINUX_BREAKPAD)
namespace {
// Comma-separated list of loaded plugin filenames.
std::string loaded_plugin_list;
}  // anonymous namespace
#endif

namespace NPAPI {

base::LazyInstance<PluginList> g_singleton(base::LINKER_INITIALIZED);

// static
PluginList* PluginList::Singleton() {
  return g_singleton.Pointer();
}

bool PluginList::PluginsLoaded() {
  AutoLock lock(lock_);
  return plugins_loaded_;
}

void PluginList::ResetPluginsLoaded() {
  AutoLock lock(lock_);
  plugins_loaded_ = false;
}

void PluginList::AddExtraPluginPath(const FilePath& plugin_path) {
  AutoLock lock(lock_);
  extra_plugin_paths_.push_back(plugin_path);
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
  AutoLock lock(lock_);
  extra_plugin_dirs_.push_back(plugin_dir);
}

void PluginList::RegisterInternalPlugin(const PluginVersionInfo& info) {
  AutoLock lock(lock_);
  internal_plugins_.push_back(info);
}

bool PluginList::ReadPluginInfo(const FilePath &filename,
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

  bool ret = PluginLib::ReadWebPluginInfo(filename, info);

#if defined(USE_LINUX_BREAKPAD)
  if (ret)
    loaded_plugin_list.append(filename.BaseName().value() + ",");
#endif

  return ret;
}

bool PluginList::CreateWebPluginInfo(const PluginVersionInfo& pvi,
                                     WebPluginInfo* info) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  SplitString(WideToUTF8(pvi.mime_types), '|', &mime_types);
  SplitString(WideToUTF8(pvi.file_extensions), '|', &file_extensions);
  SplitString(pvi.type_descriptions, '|', &descriptions);

  info->mime_types.clear();

  if (mime_types.empty())
    return false;

  info->name = pvi.product_name;
  info->desc = pvi.file_description;
  info->version = pvi.file_version;
  info->path = pvi.path;

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // On Windows, the description likely has a list of file extensions
      // embedded in it (e.g. "SurfWriter file (*.swr)"). Remove an extension
      // list from the description if it is present.
      size_t ext = mime_type.description.find(L"(*");
      if (ext != std::wstring::npos) {
        if (ext > 1 && mime_type.description[ext -1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    info->mime_types.push_back(mime_type);
  }

  return true;
}

PluginList::PluginList() : plugins_loaded_(false) {
  PlatformInit();

#if defined(OS_WIN)
  const PluginVersionInfo default_plugin = {
    FilePath(kDefaultPluginLibraryName),
    L"Default Plug-in",
    L"Provides functionality for installing third-party plug-ins",
    L"1",
    L"*",
    L"",
    L"",
    {
      default_plugin::NP_GetEntryPoints,
      default_plugin::NP_Initialize,
      default_plugin::NP_Shutdown
    }
  };

  internal_plugins_.push_back(default_plugin);
#endif
}

void PluginList::LoadPlugins(bool refresh) {
  // Don't want to hold the lock while loading new plugins, so we don't block
  // other methods if they're called on other threads.
  std::vector<FilePath> extra_plugin_paths;
  std::vector<FilePath> extra_plugin_dirs;
  std::vector<PluginVersionInfo> internal_plugins;
  {
    AutoLock lock(lock_);
    if (plugins_loaded_ && !refresh)
      return;

    extra_plugin_paths = extra_plugin_paths_;
    extra_plugin_dirs = extra_plugin_dirs_;
    internal_plugins = internal_plugins_;
  }

#if defined(USE_LINUX_BREAKPAD)
  loaded_plugin_list.clear();
#endif

  base::TimeTicks start_time = base::TimeTicks::Now();

  std::vector<WebPluginInfo> new_plugins;

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

  for (size_t i = 0; i < extra_plugin_paths.size(); ++i)
    LoadPlugin(extra_plugin_paths[i], &new_plugins);

  for (size_t i = 0; i < extra_plugin_dirs.size(); ++i) {
    LoadPluginsFromDir(extra_plugin_dirs[i], &new_plugins);
  }

  for (size_t i = 0; i < directories_to_scan.size(); ++i) {
    LoadPluginsFromDir(directories_to_scan[i], &new_plugins);
  }

  // Load the default plugin last.
  if (webkit_glue::IsDefaultPluginEnabled())
    LoadPlugin(FilePath(kDefaultPluginLibraryName), &new_plugins);

  base::TimeTicks end_time = base::TimeTicks::Now();
  base::TimeDelta elapsed = end_time - start_time;
  DLOG(INFO) << "Loaded plugin list in " << elapsed.InMilliseconds() << " ms.";

  AutoLock lock(lock_);
  plugins_ = new_plugins;
  plugins_loaded_ = true;
}

void PluginList::LoadPlugin(const FilePath &path,
                            std::vector<WebPluginInfo>* plugins) {
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

bool PluginList::FindPlugin(const std::string& mime_type,
                            bool allow_wildcard,
                            WebPluginInfo* info) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  LoadPlugins(false);
  AutoLock lock(lock_);
  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsType(plugins_[i], mime_type, allow_wildcard)) {
      *info = plugins_[i];
      return true;
    }
  }

  return false;
}

bool PluginList::FindPlugin(const GURL &url,
                            std::string* actual_mime_type,
                            WebPluginInfo* info) {
  LoadPlugins(false);
  AutoLock lock(lock_);
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot == std::string::npos)
    return false;

  std::string extension = StringToLowerASCII(std::string(path, last_dot+1));

  for (size_t i = 0; i < plugins_.size(); ++i) {
    if (SupportsExtension(plugins_[i], extension, actual_mime_type)) {
      *info = plugins_[i];
      return true;
    }
  }

  return false;
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

bool PluginList::GetPluginInfo(const GURL& url,
                               const std::string& mime_type,
                               bool allow_wildcard,
                               WebPluginInfo* info,
                               std::string* actual_mime_type) {
  bool found = FindPlugin(mime_type, allow_wildcard, info);
  if (!found || (info->path.value() == kDefaultPluginLibraryName)) {
    WebPluginInfo info2;
    if (FindPlugin(url, actual_mime_type, &info2)) {
      found = true;
      *info = info2;
    }
  }

  return found;
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

void PluginList::Shutdown() {
  // TODO
}

#if defined(USE_LINUX_BREAKPAD)
// static
// WARNING: This is called when we are in the middle of a browser crash.
// This function may not call into libc nor allocate memory.
std::string* PluginList::GetLoadedPlugins() {
  return &loaded_plugin_list;
}
#endif

}  // namespace NPAPI
