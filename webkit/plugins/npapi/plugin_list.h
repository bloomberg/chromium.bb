// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_LIST_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_LIST_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "third_party/npapi/bindings/nphostapi.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/webkit_plugins_export.h"
#include "webkit/plugins/webplugininfo.h"

class GURL;

namespace base {

template <typename T>
struct DefaultLazyInstanceTraits;

}  // namespace base

namespace webkit {
namespace npapi {

// This struct holds entry points into a plugin.  The entry points are
// slightly different between Win/Mac and Unixes.  Note that the interface for
// querying plugins is synchronous and it is preferable to use a higher-level
// asynchronous information to query information.
struct PluginEntryPoints {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  NP_GetEntryPointsFunc np_getentrypoints;
#endif
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// The PluginList is responsible for loading our NPAPI based plugins. It does
// so in whatever manner is appropriate for the platform. On Windows, it loads
// plugins from a known directory by looking for DLLs which start with "NP",
// and checking to see if they are valid NPAPI libraries. On the Mac, it walks
// the machine-wide and user plugin directories and loads anything that has
// the correct types. On Linux, it walks the plugin directories as well
// (e.g. /usr/lib/browser-plugins/).
// This object is thread safe.
class WEBKIT_PLUGINS_EXPORT PluginList {
 public:
  // Gets the one instance of the PluginList.
  static PluginList* Singleton();

  // Returns true if we're in debug-plugin-loading mode. This is controlled
  // by a command line switch.
  static bool DebugPluginLoading();

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  void RefreshPlugins();

  // Add/Remove an extra plugin to load when we actually do the loading.  Must
  // be called before the plugins have been loaded.
  void AddExtraPluginPath(const FilePath& plugin_path);
  void RemoveExtraPluginPath(const FilePath& plugin_path);

  // Same as above, but specifies a directory in which to search for plugins.
  void AddExtraPluginDir(const FilePath& plugin_dir);

  // Get the ordered list of directories from which to load plugins
  void GetPluginDirectories(std::vector<FilePath>* plugin_dirs);

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // If |add_at_beginning| is true the plugin will be added earlier in
  // the list so that it can override the MIME types of older registrations.
  void RegisterInternalPlugin(const webkit::WebPluginInfo& info,
                              bool add_at_beginning);

  // This second version is for "plugins" that have been compiled directly into
  // the binary -- callers must provide the plugin information and the entry
  // points.
  void RegisterInternalPluginWithEntryPoints(
      const webkit::WebPluginInfo& info,
      bool add_at_beginning,
      const PluginEntryPoints& entry_points);

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  //
  // This is generally only necessary for tests.
  void UnregisterInternalPlugin(const FilePath& path);

  // Gets a list of all the registered internal plugins.
  void GetInternalPlugins(std::vector<webkit::WebPluginInfo>* plugins);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".  If it's an
  // internal plugin, "entry_points" is filled in as well with a
  // internally-owned PluginEntryPoints pointer.
  // Returns false if the library couldn't be found, or if it's not a plugin.
  bool ReadPluginInfo(const FilePath& filename,
                      webkit::WebPluginInfo* info,
                      const PluginEntryPoints** entry_points);

  // In Windows plugins, the mime types are passed as a specially formatted list
  // of strings. This function parses those strings into a WebPluginMimeType
  // vector.
  // TODO(evan): move this code into plugin_list_win.
  static bool ParseMimeTypes(
      const std::string& mime_types,
      const std::string& file_extensions,
      const string16& mime_type_descriptions,
      std::vector<webkit::WebPluginMimeType>* parsed_mime_types);

  // Get all the plugins synchronously.
  void GetPlugins(std::vector<webkit::WebPluginInfo>* plugins);

  // Returns true if the list of plugins is cached and is copied into the out
  // pointer; returns false if the plugin list needs to be refreshed.
  virtual bool GetPluginsIfNoRefreshNeeded(
      std::vector<webkit::WebPluginInfo>* plugins);

  // Returns a list in |info| containing plugins that are found for
  // the given url and mime type (including disabled plugins, for
  // which |info->enabled| is false).  The mime type which corresponds
  // to the URL is optionally returned back in |actual_mime_types| (if
  // it is non-NULL), one for each of the plugin info objects found.
  // The |allow_wildcard| parameter controls whether this function
  // returns plugins which support wildcard mime types (* as the mime
  // type).  The |info| parameter is required to be non-NULL.  The
  // list is in order of "most desirable" to "least desirable".
  // If |use_stale| is NULL, this will load the plug-in list if necessary.
  // If it is not NULL, the plug-in list will not be loaded, and |*use_stale|
  // will be true iff the plug-in list was stale.
  void GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          bool* use_stale,
                          std::vector<webkit::WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types);

  // Populates the given vector with all available plugin groups. If
  // |load_if_necessary| is true, this will potentially load the plugin list
  // synchronously.
  void GetPluginGroups(bool load_if_necessary,
                       std::vector<PluginGroup>* plugin_groups);

  // Returns a copy of the PluginGroup corresponding to the given WebPluginInfo.
  // The caller takes ownership of the returned PluginGroup.
  PluginGroup* GetPluginGroup(const webkit::WebPluginInfo& web_plugin_info);

  // Returns the name of the PluginGroup with the given identifier.
  // If no such group exists, an empty string is returned.
  string16 GetPluginGroupName(const std::string& identifier);

  // Load a specific plugin with full path. Return true iff loading the plug-in
  // was successful.
  bool LoadPlugin(const FilePath& filename,
                  ScopedVector<PluginGroup>* plugin_groups,
                  webkit::WebPluginInfo* plugin_info);

  // The following functions are used to support probing for WebPluginInfo
  // using a different instance of this class.

  // Computes a list of all plugins to potentially load from all sources.
  void GetPluginPathsToLoad(std::vector<FilePath>* plugin_paths);

  // Returns the list of hardcoded plug-in groups for testing.
  const std::vector<PluginGroup*>& GetHardcodedPluginGroups() const;

  // Clears the internal list of PluginGroups and copies them from the vector.
  void SetPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

  void set_will_load_plugins_callback(const base::Closure& callback);

  virtual ~PluginList();

 protected:
  // This constructor is used in unit tests to override the platform-dependent
  // real-world plugin group definitions with custom ones.
  PluginList(const PluginGroupDefinition* definitions, size_t num_definitions);

  // Adds the given WebPluginInfo to its corresponding group, creating it if
  // necessary, and returns the group.
  PluginGroup* AddToPluginGroups(const webkit::WebPluginInfo& web_plugin_info,
                                 ScopedVector<PluginGroup>* plugin_groups);

 private:
  friend class PluginListTest;
  friend struct base::DefaultLazyInstanceTraits<PluginList>;
  FRIEND_TEST_ALL_PREFIXES(PluginGroupTest, PluginGroupDefinition);

  // Constructors are private for singletons.
  PluginList();

  // Creates PluginGroups for the hardcoded group definitions, and stores them
  // in |hardcoded_plugin_groups_|.
  void AddHardcodedPluginGroups(const PluginGroupDefinition* group_definitions,
                                size_t num_group_definitions);

  // Creates a new PluginGroup either from a hardcoded group definition, or from
  // the plug-in information.
  // Caller takes ownership of the returned PluginGroup.
  PluginGroup* CreatePluginGroup(
      const webkit::WebPluginInfo& web_plugin_info) const;

  // Implements all IO dependent operations of the LoadPlugins method so that
  // test classes can mock these out.
  virtual void LoadPluginsInternal(ScopedVector<PluginGroup>* plugin_groups);

  // Load all plugins from the default plugins directory.
  void LoadPlugins();

  // Walks a directory and produces a list of all the plugins to potentially
  // load in that directory.
  void GetPluginsInDir(const FilePath& path, std::vector<FilePath>* plugins);

  // Returns true if we should load the given plugin, or false otherwise.
  // |plugins| is the list of plugins we have crawled in the current plugin
  // loading run.
  bool ShouldLoadPlugin(const webkit::WebPluginInfo& info,
                        ScopedVector<PluginGroup>* plugins);

  // Returns true if the plugin supports |mime_type|. |mime_type| should be all
  // lower case.
  bool SupportsType(const webkit::WebPluginInfo& plugin,
                    const std::string& mime_type,
                    bool allow_wildcard);

  // Returns true if the given plugin supports a given file extension.
  // |extension| should be all lower case. If |mime_type| is not NULL, it will
  // be set to the MIME type if found. The MIME type which corresponds to the
  // extension is optionally returned back.
  bool SupportsExtension(const webkit::WebPluginInfo& plugin,
                         const std::string& extension,
                         std::string* actual_mime_type);

  //
  // Platform functions
  //

  // Do any initialization.
  void PlatformInit();

  //
  // Command-line switches
  //

#if defined(OS_WIN)
  // true if we shouldn't load the new WMP plugin.
  bool dont_load_new_wmp_;

  // Gets plugin paths registered under HKCU\Software\MozillaPlugins and
  // HKLM\Software\MozillaPlugins.
  void GetPluginPathsFromRegistry(std::vector<FilePath>* plugins);
#endif

  //
  // Internals
  //

  // If true, we reload plugins even if they've been loaded already.
  bool plugins_need_refresh_;

  // Extra plugin paths that we want to search when loading.
  std::vector<FilePath> extra_plugin_paths_;

  // Extra plugin directories that we want to search when loading.
  std::vector<FilePath> extra_plugin_dirs_;

  struct InternalPlugin {
    webkit::WebPluginInfo info;
    PluginEntryPoints entry_points;
  };
  // Holds information about internal plugins.
  std::vector<InternalPlugin> internal_plugins_;

  // Holds the currently available plugin groups.
  ScopedVector<PluginGroup> plugin_groups_;

  // Holds the hardcoded definitions of well-known plug-ins.
  // This should only be modified during construction of the PluginList.
  ScopedVector<PluginGroup> hardcoded_plugin_groups_;

  // Callback that is invoked whenever the PluginList will reload the plugins.
  base::Closure will_load_plugins_callback_;

  // Need synchronization for the above members since this object can be
  // accessed on multiple threads.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PluginList);
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_LIST_H_
