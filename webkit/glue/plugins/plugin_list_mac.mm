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
  OSErr err = FSFindFolder(user ? kLocalDomain : kUserDomain,
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

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info,
                                  std::vector<WebPluginInfo>* plugins) {
  // The Gears plugin is Safari-specific, and causes crashes, so don't load it.
  for (std::vector<WebPluginMimeType>::const_iterator i =
           info.mime_types.begin(); i != info.mime_types.end(); ++i) {
    if (i->mime_type == "application/x-googlegears")
      return false;
  }

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
