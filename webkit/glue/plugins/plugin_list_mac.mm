// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#import <Foundation/Foundation.h>

#include "base/file_util.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "webkit/glue/plugins/plugin_lib.h"

namespace {

void GetPluginCommonDirectory(std::vector<FilePath>* plugin_dirs,
                              bool user) {
  // Note that there are no NSSearchPathDirectory constants for these
  // directories so we can't use Cocoa's NSSearchPathForDirectoriesInDomains().
  // Interestingly, Safari hard-codes the location (see
  // WebKit/WebKit/mac/Plugins/WebPluginDatabase.mm's +_defaultPlugInPaths).
  FSRef ref;
  OSErr err = FSFindFolder(user ? kUserDomain : kLocalDomain,
                           kInternetPlugInFolderType, false, &ref);

  if (err)
    return;

  plugin_dirs->push_back(FilePath(mac_util::PathFromFSRef(ref)));
}

void GetPluginPrivateDirectory(std::vector<FilePath>* plugin_dirs) {
  NSString* plugin_path = [[NSBundle mainBundle] builtInPlugInsPath];
  if (!plugin_path)
    return;

  plugin_dirs->push_back(FilePath([plugin_path fileSystemRepresentation]));
}

}  // namespace

namespace NPAPI
{

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // Load from the user's area
  GetPluginCommonDirectory(plugin_dirs, true);

  // Load from the machine-wide area
  GetPluginCommonDirectory(plugin_dirs, false);

  // Load any bundled plugins
  GetPluginPrivateDirectory(plugin_dirs);
}

void PluginList::LoadPluginsFromDir(const FilePath &path,
                                    std::vector<WebPluginInfo>* plugins) {
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    LoadPlugin(path, plugins);
  }
}

// Returns true if |array| contains a string matching |test_string|.
static bool ArrayContainsString(const char** array, unsigned int array_size,
                                const std::string& test_string) {
  // We're dealing with very small lists, so just walk in order; if we someday
  // end up with very big blacklists or whitelists we can revisit the approach.
  for (unsigned int i = 0; i < array_size; ++i) {
    if (test_string == array[i])
      return true;
  }
  return false;
}

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info,
                                  std::vector<WebPluginInfo>* plugins) {
  // Plugins that we know don't work at all.
  const char* blacklisted_plugin_mimes[] = {
    "application/x-director",             // Crashes during initialization.
    "application/x-googlegears",          // Safari-specific.
    "application/x-id-quakelive",         // Crashes on load.
    "application/x-vnd.movenetworks.qm",  // Crashes on Snow Leopard.
    "application/vnd.o3d.auto",           // Doesn't render, and having it
                                          // detected can prevent fallbacks.
  };
  // In the case of plugins that share MIME types, we have to blacklist by name.
  const char* blacklisted_plugin_names[] = {
    // Blacklisted for now since having it non-functional but trying to handle
    // PDFs is worse than not having it (PDF content would otherwise be
    // downloaded or handled by QuickTime).
    "PDF Browser Plugin",
  };

  // Plugins that we know are working reasonably well.
  const char* whitelisted_plugin_mimes[] = {
    "application/googletalk",
    "application/x-picasa-detect",
    "application/x-shockwave-flash",
    "application/x-silverlight",
    "application/x-webkit-test-netscape",
    "video/quicktime",
  };

  // Start with names.
  std::string plugin_name = WideToUTF8(info.name);
  if (ArrayContainsString(blacklisted_plugin_names,
                          arraysize(blacklisted_plugin_names), plugin_name)) {
    return false;
  }
  // Then check mime types.
  bool whitelisted = false;
  for (std::vector<WebPluginMimeType>::const_iterator i =
           info.mime_types.begin(); i != info.mime_types.end(); ++i) {
    if (ArrayContainsString(blacklisted_plugin_mimes,
                            arraysize(blacklisted_plugin_mimes),
                            i->mime_type)) {
      return false;
    }
    if (ArrayContainsString(whitelisted_plugin_mimes,
                            arraysize(whitelisted_plugin_mimes),
                            i->mime_type)) {
      whitelisted = true;
      break;
    }
  }

#if OS_MACOSX_BLACKLIST_PLUGINS_BY_DEFAULT
  if (!whitelisted)
    return false;
#endif

  // Hierarchy check
  // (we're loading plugins hierarchically from Library folders, so plugins we
  //  encounter earlier must override plugins we encounter later)
  for (size_t i = 0; i < plugins->size(); ++i) {
    if ((*plugins)[i].path.BaseName() == info.path.BaseName()) {
      return false;  // We already have a loaded plugin higher in the hierarchy.
    }
  }

  return true;
}

} // namespace NPAPI
