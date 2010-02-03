// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_list.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"

namespace {

// Return true if we're in debug-plugin-loading mode.
bool DebugPluginLoading() {
  static const char kDebugPluginLoading[] = "debug-plugin-loading";
  return CommandLine::ForCurrentProcess()->HasSwitch(kDebugPluginLoading);
}

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

// Some plugins are shells around other plugins; we prefer to use the
// real plugin directly, if it's available.  This function returns
// true if we should prefer other plugins over this one.  We'll still
// use a "undesirable" plugin if no other option is available.
bool IsUndesirablePlugin(const WebPluginInfo& info) {
  std::string filename = info.path.BaseName().value();
  return (filename.find("npcxoffice") != std::string::npos || // Crossover
          filename.find("npwrapper") != std::string::npos);   // nspluginwrapper
}

}  // anonymous namespace

namespace NPAPI {

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // See http://groups.google.com/group/chromium-dev/browse_thread/thread/7a70e5fcbac786a9
  // for discussion.
  // We first consult Chrome-specific dirs, then fall back on the logic
  // Mozilla uses.

  // Note: "extra" plugin dirs, including the Plugins subdirectory of
  // your Chrome config, are examined before these.  See the logic
  // related to extra_plugin_dirs in plugin_list.cc.

  // The Chrome binary dir + "plugins/".
  FilePath dir;
  PathService::Get(base::DIR_EXE, &dir);
  plugin_dirs->push_back(dir.Append("plugins"));

  // Mozilla code to reference:
  // http://mxr.mozilla.org/firefox/ident?i=NS_APP_PLUGINS_DIR_LIST
  // and tens of accompanying files (mxr is very helpful).
  // This code carefully matches their behavior for compat reasons.

  // 1) MOZ_PLUGIN_PATH env variable.
  const char* moz_plugin_path = getenv("MOZ_PLUGIN_PATH");
  if (moz_plugin_path) {
    std::vector<std::string> paths;
    SplitString(moz_plugin_path, ':', &paths);
    for (size_t i = 0; i < paths.size(); ++i)
      plugin_dirs->push_back(FilePath(paths[i]));
  }

  // 2) NS_USER_PLUGINS_DIR: ~/.mozilla/plugins.
  // This is a de-facto standard, so even though we're not Mozilla, let's
  // look in there too.
  const char* home = getenv("HOME");
  if (home)
    plugin_dirs->push_back(FilePath(home).Append(".mozilla/plugins"));

  // 3) NS_SYSTEM_PLUGINS_DIR:
  // This varies across different browsers and versions, so check 'em all.
  plugin_dirs->push_back(FilePath("/usr/lib/browser-plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib/mozilla/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib/firefox/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib/xulrunner-addons/plugins"));

#if defined(ARCH_CPU_64_BITS)
  // On my Ubuntu system, /usr/lib64 is a symlink to /usr/lib.
  // But a user reported on their Fedora system they are separate.
  plugin_dirs->push_back(FilePath("/usr/lib64/browser-plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib64/mozilla/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib64/firefox/plugins"));
  plugin_dirs->push_back(FilePath("/usr/lib64/xulrunner-addons/plugins"));
#endif
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
    FilePath orig_path = path;
    file_util::AbsolutePath(&path);
    if (DebugPluginLoading())
      LOG(ERROR) << "Resolved " << orig_path.value() << " -> " << path.value();

    // Flash stops working if the containing directory involves 'netscape'.
    // No joke.  So use the other path if it's better.
    static const char kFlashPlayerFilename[] = "libflashplayer.so";
    static const char kNetscapeInPath[] = "/netscape/";
    if (path.BaseName().value() == kFlashPlayerFilename &&
        path.value().find(kNetscapeInPath) != std::string::npos) {
      if (orig_path.value().find(kNetscapeInPath) == std::string::npos) {
        // Go back to the old path.
        path = orig_path;
      } else {
        LOG(ERROR) << "Flash misbehaves when used from a directory containing "
                   << kNetscapeInPath << ", so skipping " << orig_path.value();
        continue;
      }
    }

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
    if (skip) {
      if (DebugPluginLoading())
        LOG(ERROR) << "Skipping duplicate instance of " << path.value();
      continue;
    }

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
  if (DebugPluginLoading()) {
    LOG(ERROR) << "Considering " << info.path.value()
              << " (" << info.name << ")";
  }
  if (IsUndesirablePlugin(info)) {
    if (DebugPluginLoading())
      LOG(ERROR) << info.path.value() << " is undesirable.";

    // See if we have a better version of this plugin.
    for (size_t i = 0; i < plugins->size(); ++i) {
      if (plugins->at(i).name == info.name &&
          !IsUndesirablePlugin(plugins->at(i))) {
        // Skip the current undesirable one so we can use the better one
        // we just found.
        if (DebugPluginLoading()) {
          LOG(ERROR) << "Skipping " << info.path.value() << ", preferring "
                    << plugins->at(i).path.value();
        }
        return false;
      }
    }
  }

  // TODO(evanm): prefer the newest version of flash, etc. here?

  if (DebugPluginLoading())
    LOG(ERROR) << "Using " << info.path.value();

  return true;
}

} // namespace NPAPI
