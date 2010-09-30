// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_
#define WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_

#include <set>
#include <string>
#include <vector>
#include <set>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "third_party/npapi/bindings/nphostapi.h"
#include "webkit/glue/plugins/webplugininfo.h"

class GURL;

namespace base {

template <typename T>
struct DefaultLazyInstanceTraits;

}  // namespace base

namespace NPAPI {

#define kDefaultPluginLibraryName FILE_PATH_LITERAL("default_plugin")
#define kGearsPluginLibraryName FILE_PATH_LITERAL("gears")

class PluginInstance;

// This struct holds entry points into a plugin.  The entry points are
// slightly different between Win/Mac and Unixes.
struct PluginEntryPoints {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  NP_GetEntryPointsFunc np_getentrypoints;
#endif
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// This struct fully describes a plugin. For external plugins, it's read in from
// the version info of the dll; For internal plugins, it's predefined and
// includes addresses of entry functions. (Yes, it's Win32 NPAPI-centric, but
// it'll do for holding descriptions of internal plugins cross-platform.)
struct PluginVersionInfo {
  FilePath path;
  // Info about the plugin itself.
  std::wstring product_name;
  std::wstring file_description;
  std::wstring file_version;
  // Info about the data types that the plugin supports.
  std::wstring mime_types;
  std::wstring file_extensions;
  std::wstring type_descriptions;
  // Entry points for internal plugins.  Pointers are NULL for external plugins.
  PluginEntryPoints entry_points;
};

typedef void (*LoadPluginsFromDiskHookFunc)();

// The PluginList is responsible for loading our NPAPI based plugins. It does
// so in whatever manner is appropriate for the platform. On Windows, it loads
// plugins from a known directory by looking for DLLs which start with "NP",
// and checking to see if they are valid NPAPI libraries. On the Mac, it walks
// the machine-wide and user plugin directories and loads anything that has
// the correct types. On Linux, it walks the plugin directories as well
// (e.g. /usr/lib/browser-plugins/).
// This object is thread safe.
class PluginList {
 public:
  // Gets the one instance of the PluginList.
  static PluginList* Singleton();

  // Set a hook that is called whenever we load plugins from the disk.
  static void SetPluginLoadHook(LoadPluginsFromDiskHookFunc hook);

  // Returns true if we're in debug-plugin-loading mode. This is controlled
  // by a command line switch.
  static bool DebugPluginLoading();

  // Returns true iff the plugin list has been loaded already.
  bool PluginsLoaded();

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  void RefreshPlugins();

  // Add/Remove an extra plugin to load when we actually do the loading.  Must
  // be called before the plugins have been loaded.
  void AddExtraPluginPath(const FilePath& plugin_path);
  void RemoveExtraPluginPath(const FilePath& plugin_path);

  // Same as above, but specifies a directory in which to search for plugins.
  void AddExtraPluginDir(const FilePath& plugin_dir);

  // Register an internal plugin with the specified plugin information and
  // function pointers.  An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  void RegisterInternalPlugin(const PluginVersionInfo& info);

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  //
  // This is generally only necessary for tests.
  void UnregisterInternalPlugin(const FilePath& path);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".  If it's an
  // internal plugin, "entry_points" is filled in as well with a
  // internally-owned PluginEntryPoints pointer.
  // Returns false if the library couldn't be found, or if it's not a plugin.
  bool ReadPluginInfo(const FilePath& filename,
                      WebPluginInfo* info,
                      const PluginEntryPoints** entry_points);

  // Populate a WebPluginInfo from a PluginVersionInfo.
  static bool CreateWebPluginInfo(const PluginVersionInfo& pvi,
                                  WebPluginInfo* info);

  // Shutdown all plugins.  Should be called at process teardown.
  void Shutdown();

  // Get all the plugins.
  void GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Get all the enabled plugins.
  void GetEnabledPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Returns true if a plugin is found for the given url and mime type
  // (including disabled plugins, for which |info->enabled| is false).
  // The mime type which corresponds to the URL is optionally returned
  // back.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type).
  bool GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     WebPluginInfo* info,
                     std::string* actual_mime_type);

  // Get plugin info by plugin path (including disabled plugins). Returns true
  // if the plugin is found and WebPluginInfo has been filled in |info|.
  bool GetPluginInfoByPath(const FilePath& plugin_path,
                           WebPluginInfo* info);

  // Load a specific plugin with full path.
  void LoadPlugin(const FilePath& filename,
                  std::vector<WebPluginInfo>* plugins);

  // Enable a specific plugin, specified by path. Returns |true| iff a plugin
  // currently in the plugin list was actually enabled as a result; regardless
  // of return value, if a plugin is found in the future with the given name, it
  // will be enabled. Note that plugins are enabled by default as far as
  // |PluginList| is concerned.
  bool EnablePlugin(const FilePath& filename);

  // Disable a specific plugin, specified by path. Returns |true| iff a plugin
  // currently in the plugin list was actually disabled as a result; regardless
  // of return value, if a plugin is found in the future with the given name, it
  // will be disabled.
  bool DisablePlugin(const FilePath& filename);

 private:
  // Constructors are private for singletons
  PluginList();

  // Load all plugins from the default plugins directory
  void LoadPlugins(bool refresh);

  // Load all plugins from a specific directory.
  // |plugins| is updated with loaded plugin information.
  // |visited_plugins| is updated with paths to all plugins that were considered
  //   (including those we didn't load)
  void LoadPluginsFromDir(const FilePath& path,
                          std::vector<WebPluginInfo>* plugins,
                          std::set<FilePath>* visited_plugins);

  // Returns true if we should load the given plugin, or false otherwise.
  // plugins is the list of plugins we have crawled in the current plugin
  // loading run.
  bool ShouldLoadPlugin(const WebPluginInfo& info,
                        std::vector<WebPluginInfo>* plugins);

  // Find a plugin by mime type; only searches enabled plugins.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  bool FindPlugin(const std::string &mime_type,
                  bool allow_wildcard,
                  WebPluginInfo* info);

  // Just like |FindPlugin| but it only looks at the disabled plug-ins.
  bool FindDisabledPlugin(const std::string &mime_type,
                          bool allow_wildcard,
                          WebPluginInfo* info);

  // Find a plugin by extension; only searches enabled plugins. Returns the
  // corresponding mime type.
  bool FindPlugin(const GURL &url,
                  std::string* actual_mime_type,
                  WebPluginInfo* info);

  // Returns true if the given WebPluginInfo supports "mime-type".
  // mime_type should be all lower case.
  static bool SupportsType(const WebPluginInfo& info,
                           const std::string &mime_type,
                           bool allow_wildcard);

  // Returns true if the given WebPluginInfo supports a given file extension.
  // extension should be all lower case.
  // If mime_type is not NULL, it will be set to the mime type if found.
  // The mime type which corresponds to the extension is optionally returned
  // back.
  static bool SupportsExtension(const WebPluginInfo& info,
                                const std::string &extension,
                                std::string* actual_mime_type);

  //
  // Platform functions
  //

  // Do any initialization.
  void PlatformInit();

  // Get the ordered list of directories from which to load plugins
  void GetPluginDirectories(std::vector<FilePath>* plugin_dirs);

  //
  // Command-line switches
  //

#if defined(OS_WIN)
  // true if we shouldn't load the new WMP plugin.
  bool dont_load_new_wmp_;

  // Loads plugins registered under HKCU\Software\MozillaPlugins and
  // HKLM\Software\MozillaPlugins.
  void LoadPluginsFromRegistry(std::vector<WebPluginInfo>* plugins,
                               std::set<FilePath>* visited_plugins);
#endif

  //
  // Internals
  //

  bool plugins_loaded_;

  // If true, we reload plugins even if they've been loaded already.
  bool plugins_need_refresh_;

  // Contains information about the available plugins.
  std::vector<WebPluginInfo> plugins_;

  // Extra plugin paths that we want to search when loading.
  std::vector<FilePath> extra_plugin_paths_;

  // Extra plugin directories that we want to search when loading.
  std::vector<FilePath> extra_plugin_dirs_;

  // Holds information about internal plugins.
  std::vector<PluginVersionInfo> internal_plugins_;

  // Path names of plugins to disable (the default is to enable them all).
  std::set<FilePath> disabled_plugins_;

  // Need synchronization for the above members since this object can be
  // accessed on multiple threads.
  Lock lock_;

  friend struct base::DefaultLazyInstanceTraits<PluginList>;

  DISALLOW_COPY_AND_ASSIGN(PluginList);
};

}  // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_
