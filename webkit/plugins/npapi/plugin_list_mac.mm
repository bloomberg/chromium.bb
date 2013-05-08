// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#import <Foundation/Foundation.h>

#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "webkit/plugins/npapi/plugin_lib.h"

namespace webkit {
namespace npapi {

namespace {

void GetPluginCommonDirectory(std::vector<base::FilePath>* plugin_dirs,
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

  plugin_dirs->push_back(base::FilePath(base::mac::PathFromFSRef(ref)));
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

  // Versions of Flip4Mac 2.3 before 2.3.6 often hang the renderer, so don't
  // load them.
  if (StartsWith(info.name, ASCIIToUTF16("Flip4Mac Windows Media"), false) &&
      StartsWith(info.version, ASCIIToUTF16("2.3"), false)) {
    std::vector<base::string16> components;
    base::SplitString(info.version, '.', &components);
    int bugfix_version = 0;
    return (components.size() >= 3 &&
            base::StringToInt(components[2], &bugfix_version) &&
            bugfix_version < 6);
  }

  return false;
}

}  // namespace

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<base::FilePath>* plugin_dirs) {
  // Load from the user's area
  GetPluginCommonDirectory(plugin_dirs, true);

  // Load from the machine-wide area
  GetPluginCommonDirectory(plugin_dirs, false);
}

void PluginList::GetPluginsInDir(
    const base::FilePath& path, std::vector<base::FilePath>* plugins) {
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    plugins->push_back(path);
  }
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info,
    std::vector<webkit::WebPluginInfo>* plugins) {
  return !IsBlacklistedPlugin(info);
}

}  // namespace npapi
}  // namespace webkit
