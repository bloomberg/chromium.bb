// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_LIB_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_LIB_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/webkit_plugins_export.h"

class FilePath;

namespace webkit {
namespace npapi {

class PluginInstance;

// A PluginLib is a single NPAPI Plugin Library, and is the lifecycle
// manager for new PluginInstances.
class WEBKIT_PLUGINS_EXPORT PluginLib : public base::RefCounted<PluginLib> {
 public:
  static PluginLib* CreatePluginLib(const FilePath& filename);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".
  // Returns false if the library couldn't be found, or if it's not a plugin.
  static bool ReadWebPluginInfo(const FilePath& filename,
                                webkit::WebPluginInfo* info);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Parse the result of an NP_GetMIMEDescription() call.
  // This API is only used on Unixes, and is exposed here for testing.
  static void ParseMIMEDescription(const std::string& description,
      std::vector<webkit::WebPluginMimeType>* mime_types);

  // Extract a version number from a description string.
  // This API is only used on Unixes, and is exposed here for testing.
  static void ExtractVersionString(const std::string& version,
                                   webkit::WebPluginInfo* info);
#endif

  // Unloads all the loaded plugin libraries and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Shuts down all loaded plugin instances.
  static void ShutdownAllPlugins();

  // Get the Plugin's function pointer table.
  NPPluginFuncs* functions();

  // Creates a new instance of this plugin.
  PluginInstance* CreateInstance(const std::string& mime_type);

  // Called by the instance when the instance is tearing down.
  void CloseInstance();

  // Gets information about this plugin and the mime types that it
  // supports.
  const webkit::WebPluginInfo& plugin_info() { return web_plugin_info_; }

  bool internal() { return internal_; }

  //
  // NPAPI functions
  //

  // NPAPI method to initialize a Plugin.
  // Initialize can be safely called multiple times
  NPError NP_Initialize();

  // NPAPI method to shutdown a Plugin.
  void NP_Shutdown(void);

  // NPAPI method to clear locally stored data (LSO's or "Flash cookies").
  NPError NP_ClearSiteData(const char* site, uint64 flags, uint64 max_age);

  // NPAPI method to get a NULL-terminated list of all sites under which data
  // is stored.
  char** NP_GetSitesWithData();

  int instance_count() const { return instance_count_; }

  // Prevents the library code from being unload when Unload() is called (since
  // some plugins crash if unloaded).
  void PreventLibraryUnload();

  // Indicates whether plugin unload can be deferred.
  void set_defer_unload(bool defer_unload) {
    defer_unload_ = defer_unload;
  }

  // protected for testability.
 protected:
  friend class base::RefCounted<PluginLib>;

  // Creates a new PluginLib.
  // |entry_points| is non-NULL for internal plugins.
  PluginLib(const webkit::WebPluginInfo& info,
            const PluginEntryPoints* entry_points);

  virtual ~PluginLib();

  // Attempts to load the plugin from the library.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloads the plugin library.
  void Unload();

  // Shutdown the plugin library.
  void Shutdown();

 private:
  bool internal_;  // True for plugins that are built-in into chrome binaries.
  webkit::WebPluginInfo web_plugin_info_;  // Supported mime types, description
  base::NativeLibrary library_;  // The opened library reference.
  NPPluginFuncs plugin_funcs_;  // The struct of plugin side functions.
  bool initialized_;  // Is the plugin initialized?
  NPSavedData *saved_data_;  // Persisted plugin info for NPAPI.
  int instance_count_;  // Count of plugins in use.
  bool skip_unload_;  // True if library_ should not be unloaded.

  // Function pointers to entry points into the plugin.
  PluginEntryPoints entry_points_;

  // Set to true if unloading of the plugin dll is to be deferred.
  bool defer_unload_;

  DISALLOW_COPY_AND_ASSIGN(PluginLib);
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_LIB_H_
