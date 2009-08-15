// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace NPAPI {

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // Mozilla code to reference:
  // http://mxr.mozilla.org/firefox/ident?i=NS_APP_PLUGINS_DIR_LIST
  // and tens of accompanying files (mxr is very helpful).
  // This code carefully matches their behavior for compat reasons.

  // 1) MOZ_PLUGIN_PATH env variable.
  const char* moz_plugin_path = getenv("MOZ_PLUGIN_PATH");
  if (moz_plugin_path)
    plugin_dirs->push_back(FilePath(moz_plugin_path));

  // 2) NS_USER_PLUGINS_DIR: ~/.mozilla/plugins.
  // This is a de-facto standard, so even though we're not Mozilla, let's
  // look in there too.
  const char* home = getenv("HOME");
  if (home)
    plugin_dirs->push_back(FilePath(home).Append(".mozilla/plugins"));
  // TODO(evanm): maybe consult our own plugins dir, like
  // ~/.config/chromium/Plugins?

  // 3) NS_APP_PLUGINS_DIR: the binary dir + "plugins/".
  FilePath dir;
  PathService::Get(base::DIR_EXE, &dir);
  plugin_dirs->push_back(dir.Append("plugins"));

  // 4) NS_SYSTEM_PLUGINS_DIR:
  // TODO(evanm): when we support 64-bit platforms, we'll need to fix this
  // to be conditional.
  COMPILE_ASSERT(sizeof(int)==4, fix_system_lib_path);
  plugin_dirs->push_back(FilePath("/usr/lib/mozilla/plugins"));
}

void PluginList::LoadPluginsFromDir(const FilePath& path,
                                    std::vector<WebPluginInfo>* plugins) {
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    // Skip over Mozilla .xpt files.
    if (!path.MatchesExtension(FILE_PATH_LITERAL(".xpt")))
      LoadPlugin(path, plugins);
  }
}

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info,
                                  std::vector<WebPluginInfo>* plugins) {
  // The equivalent Windows code verifies we haven't loaded a newer version
  // of the same plugin, and then blacklists some known bad plugins.
  // The equivalent Mac code verifies that plugins encountered first in the
  // plugin list clobber later entries.
  // TODO(evanm): figure out which behavior is appropriate for Linux.
  // We don't need either yet as I'm just testing with Flash for now.
  return true;
}

void PluginList::LoadInternalPlugins(std::vector<WebPluginInfo>* plugins) {
  // none for now
}

} // namespace NPAPI
