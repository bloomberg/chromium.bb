// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#import <Foundation/Foundation.h>

#include "base/file_util.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
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

// Returns true if the plugin should be prevented from loading.
bool IsBlacklistedPlugin(const WebPluginInfo& info) {
  // We blacklist Gears by included MIME type, since that is more stable than
  // its name. Be careful about adding any more plugins to this list though,
  // since it's easy to accidentally blacklist plugins that support lots of
  // MIME types.
  for (std::vector<WebPluginMimeType>::const_iterator i =
           info.mime_types.begin(); i != info.mime_types.end(); ++i) {
    // The Gears plugin is Safari-specific, so don't load it.
    if (i->mime_type == "application/x-googlegears")
      return true;
  }

  return false;
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

  // Load any bundled plugins (deprecated)
  // TODO(stuartmorgan): Remove this once it's not used in TestShell.
  GetPluginPrivateDirectory(plugin_dirs);
}

void PluginList::LoadPluginsFromDir(const FilePath &path,
                                    std::vector<WebPluginInfo>* plugins,
                                    std::set<FilePath>* visited_plugins) {
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    LoadPlugin(path, plugins);
    visited_plugins->insert(path);
  }
}

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info,
                                  std::vector<WebPluginInfo>* plugins) {
  if (IsBlacklistedPlugin(info))
    return false;

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
