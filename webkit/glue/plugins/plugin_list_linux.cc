// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace {

// We build up a list of files and mtimes so we can sort them.
typedef std::pair<FilePath, base::Time> FileAndTime;
typedef std::vector<FileAndTime> FileTimeList;

// Comparator used to sort by descending mtime then ascending filename.
bool CompareTime(const FileAndTime& a, const FileAndTime& b) {
  if (a.second == b.second) {
    // Fall back on filename sorting, just to make the predicate valid.
    return a.first < b.first;
  }

  // Sort by mtime, descending.
  return a.second > b.second;
}

}

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
  // This varies across different versions of Firefox, so check 'em all.
  plugin_dirs->push_back(FilePath("/usr/lib/mozilla/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib/firefox/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib/xulrunner-addons/plugins"));
}

void PluginList::LoadPluginsFromDir(const FilePath& path,
                                    std::vector<WebPluginInfo>* plugins) {
  // See ScanPluginsDirectory near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginHostImpl.cpp#5052

  // Construct and stat a list of all filenames under consideration, for
  // later sorting by mtime.
  FileTimeList files;
  file_util::FileEnumerator enumerator(path,
                                       false, // not recursive
                                       file_util::FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    // Skip over Mozilla .xpt files.
    if (path.MatchesExtension(FILE_PATH_LITERAL(".xpt")))
      continue;

    // Java doesn't like being loaded through a symlink, since it uses
    // its path to find dependent data files.
    // file_util::AbsolutePath calls through to realpath(), which resolves
    // symlinks.
    file_util::AbsolutePath(&path);

    // Get mtime.
    file_util::FileInfo info;
    if (!file_util::GetFileInfo(path, &info))
      continue;

    // Skip duplicates of the same file in our list.
    bool skip = false;
    for (size_t i = 0; i < plugins->size(); ++i) {
      if (plugins->at(i).path == path) {
        skip = true;
        break;
      }
    }
    if (skip)
      continue;

    files.push_back(std::make_pair(path, info.last_modified));
  }

  // Sort the file list by time (and filename).
  std::sort(files.begin(), files.end(), CompareTime);

  // Load the files in order.
  for (FileTimeList::const_iterator i = files.begin(); i != files.end(); ++i) {
    LoadPlugin(i->first, plugins);
  }
}

bool PluginList::ShouldLoadPlugin(const WebPluginInfo& info,
                                  std::vector<WebPluginInfo>* plugins) {
  // TODO(evanm): blacklist nspluginwrapper here?
  // TODO(evanm): prefer the newest version of flash here?

  return true;
}

void PluginList::LoadInternalPlugins(std::vector<WebPluginInfo>* plugins) {
  // none for now
}

} // namespace NPAPI
