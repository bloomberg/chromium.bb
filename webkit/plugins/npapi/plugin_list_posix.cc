// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include <algorithm>

#include "base/cpu.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"

namespace webkit {
namespace npapi {

namespace {

// We build up a list of files and mtimes so we can sort them.
typedef std::pair<base::FilePath, base::Time> FileAndTime;
typedef std::vector<FileAndTime> FileTimeList;

enum PluginQuirk {
  // No quirks - plugin is outright banned.
  PLUGIN_QUIRK_NONE = 0,
  // Plugin is using SSE2 instructions without checking for SSE2 instruction
  // support. Ban the plugin if the system has no SSE2 support.
  PLUGIN_QUIRK_MISSING_SSE2_CHECK = 1 << 0,
};

// Comparator used to sort by descending mtime then ascending filename.
bool CompareTime(const FileAndTime& a, const FileAndTime& b) {
  if (a.second == b.second) {
    // Fall back on filename sorting, just to make the predicate valid.
    return a.first < b.first;
  }

  // Sort by mtime, descending.
  return a.second > b.second;
}

// Checks to see if the current environment meets any of the condtions set in
// |quirks|. Returns true if any of the conditions are met, or if |quirks| is
// PLUGIN_QUIRK_NONE.
bool CheckQuirks(PluginQuirk quirks) {
  if (quirks == PLUGIN_QUIRK_NONE)
    return true;

  if ((quirks & PLUGIN_QUIRK_MISSING_SSE2_CHECK) != 0) {
    base::CPU cpu;
    if (!cpu.has_sse2())
      return true;
  }

  return false;
}

// Return true if |path| matches a known (file size, sha1sum) pair.
// Also check against any PluginQuirks the bad plugin may have.
// The use of the file size is an optimization so we don't have to read in
// the entire file unless we have to.
bool IsBlacklistedBySha1sumAndQuirks(const base::FilePath& path) {
  const struct BadEntry {
    int64 size;
    std::string sha1;
    PluginQuirk quirks;
  } bad_entries[] = {
    // Flash 9 r31 - http://crbug.com/29237
    { 7040080, "fa5803061125ca47846713b34a26a42f1f1e98bb", PLUGIN_QUIRK_NONE },
    // Flash 9 r48 - http://crbug.com/29237
    { 7040036, "0c4b3768a6d4bfba003088e4b9090d381de1af2b", PLUGIN_QUIRK_NONE },
    // Flash 11.2.202.236, 32-bit - http://crbug.com/140086
    { 17406436, "1e07eac912faf9426c52a288c76c3b6238f90b6b",
      PLUGIN_QUIRK_MISSING_SSE2_CHECK },
    // Flash 11.2.202.238, 32-bit - http://crbug.com/140086
    { 17410532, "e9401097e97c8443a7d9156be62184ffe1addd5c",
      PLUGIN_QUIRK_MISSING_SSE2_CHECK },
  };

  int64 size;
  if (!file_util::GetFileSize(path, &size))
    return false;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(bad_entries); i++) {
    if (bad_entries[i].size != size)
      continue;

    std::string file_content;
    if (!file_util::ReadFileToString(path, &file_content))
      continue;
    std::string sha1 = base::SHA1HashString(file_content);
    std::string sha1_readable;
    for (size_t j = 0; j < sha1.size(); j++)
      base::StringAppendF(&sha1_readable, "%02x", sha1[j] & 0xFF);
    if (bad_entries[i].sha1 == sha1_readable)
      return CheckQuirks(bad_entries[i].quirks);
  }
  return false;
}

// Some plugins are shells around other plugins; we prefer to use the
// real plugin directly, if it's available.  This function returns
// true if we should prefer other plugins over this one.  We'll still
// use a "undesirable" plugin if no other option is available.
bool IsUndesirablePlugin(const WebPluginInfo& info) {
  std::string filename = info.path.BaseName().value();
  const char* kUndesiredPlugins[] = {
    "npcxoffice",  // Crossover
    "npwrapper",   // nspluginwrapper
  };
  for (size_t i = 0; i < arraysize(kUndesiredPlugins); i++) {
    if (filename.find(kUndesiredPlugins[i]) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Return true if we shouldn't load a plugin at all.
// This is an ugly hack to blacklist Adobe Acrobat due to not supporting
// its Xt-based mainloop.
// http://code.google.com/p/chromium/issues/detail?id=38229
bool IsBlacklistedPlugin(const base::FilePath& path) {
  const char* kBlackListedPlugins[] = {
    "nppdf.so",           // Adobe PDF
  };
  std::string filename = path.BaseName().value();
  for (size_t i = 0; i < arraysize(kBlackListedPlugins); i++) {
    if (filename.find(kBlackListedPlugins[i]) != std::string::npos) {
      return true;
    }
  }
  return IsBlacklistedBySha1sumAndQuirks(path);
}

}  // namespace

void PluginList::PlatformInit() {
}

void PluginList::GetPluginDirectories(std::vector<base::FilePath>* plugin_dirs) {
  // See http://groups.google.com/group/chromium-dev/browse_thread/thread/7a70e5fcbac786a9
  // for discussion.
  // We first consult Chrome-specific dirs, then fall back on the logic
  // Mozilla uses.

  // Note: "extra" plugin dirs, including the Plugins subdirectory of
  // your Chrome config, are examined before these.  See the logic
  // related to extra_plugin_dirs in plugin_list.cc.

  // The Chrome binary dir + "plugins/".
  base::FilePath dir;
  PathService::Get(base::DIR_EXE, &dir);
  plugin_dirs->push_back(dir.Append("plugins"));

  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  // Mozilla code to reference:
  // http://mxr.mozilla.org/firefox/ident?i=NS_APP_PLUGINS_DIR_LIST
  // and tens of accompanying files (mxr is very helpful).
  // This code carefully matches their behavior for compat reasons.

  // 1) MOZ_PLUGIN_PATH env variable.
  const char* moz_plugin_path = getenv("MOZ_PLUGIN_PATH");
  if (moz_plugin_path) {
    std::vector<std::string> paths;
    base::SplitString(moz_plugin_path, ':', &paths);
    for (size_t i = 0; i < paths.size(); ++i)
      plugin_dirs->push_back(base::FilePath(paths[i]));
  }

  // 2) NS_USER_PLUGINS_DIR: ~/.mozilla/plugins.
  // This is a de-facto standard, so even though we're not Mozilla, let's
  // look in there too.
  base::FilePath home = file_util::GetHomeDir();
  if (!home.empty())
    plugin_dirs->push_back(home.Append(".mozilla/plugins"));

  // 3) NS_SYSTEM_PLUGINS_DIR:
  // This varies across different browsers and versions, so check 'em all.
  plugin_dirs->push_back(base::FilePath("/usr/lib/browser-plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/mozilla/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/firefox/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/xulrunner-addons/plugins"));

#if defined(ARCH_CPU_64_BITS)
  // On my Ubuntu system, /usr/lib64 is a symlink to /usr/lib.
  // But a user reported on their Fedora system they are separate.
  plugin_dirs->push_back(base::FilePath("/usr/lib64/browser-plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/mozilla/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/firefox/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/xulrunner-addons/plugins"));
#endif  // defined(ARCH_CPU_64_BITS)
#endif  // !defined(OS_CHROMEOS)
}

void PluginList::GetPluginsInDir(
    const base::FilePath& dir_path, std::vector<base::FilePath>* plugins) {
  // See ScanPluginsDirectory near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginHostImpl.cpp#5052

  // Construct and stat a list of all filenames under consideration, for
  // later sorting by mtime.
  FileTimeList files;
  file_util::FileEnumerator enumerator(dir_path,
                                       false,  // not recursive
                                       file_util::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    // Skip over Mozilla .xpt files.
    if (path.MatchesExtension(FILE_PATH_LITERAL(".xpt")))
      continue;

    // Java doesn't like being loaded through a symlink, since it uses
    // its path to find dependent data files.
    // file_util::AbsolutePath calls through to realpath(), which resolves
    // symlinks.
    base::FilePath orig_path = path;
    file_util::AbsolutePath(&path);
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Resolved " << orig_path.value() << " -> " << path.value();

    if (std::find(plugins->begin(), plugins->end(), path) != plugins->end()) {
      LOG_IF(ERROR, PluginList::DebugPluginLoading())
          << "Skipping duplicate instance of " << path.value();
      continue;
    }

    if (IsBlacklistedPlugin(path)) {
      LOG_IF(ERROR, PluginList::DebugPluginLoading())
          << "Skipping blacklisted plugin " << path.value();
      continue;
    }

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
        LOG_IF(ERROR, PluginList::DebugPluginLoading())
            << "Flash misbehaves when used from a directory containing "
            << kNetscapeInPath << ", so skipping " << orig_path.value();
        continue;
      }
    }

    // Get mtime.
    base::PlatformFileInfo info;
    if (!file_util::GetFileInfo(path, &info))
      continue;

    files.push_back(std::make_pair(path, info.last_modified));
  }

  // Sort the file list by time (and filename).
  std::sort(files.begin(), files.end(), CompareTime);

  // Load the files in order.
  for (FileTimeList::const_iterator i = files.begin(); i != files.end(); ++i) {
    plugins->push_back(i->first);
  }
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info, std::vector<webkit::WebPluginInfo>* plugins) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Considering " << info.path.value() << " (" << info.name << ")";

  if (IsUndesirablePlugin(info)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << info.path.value() << " is undesirable.";

    // See if we have a better version of this plugin.
    for (size_t j = 0; j < plugins->size(); ++j) {
      if ((*plugins)[j].name == info.name &&
          !IsUndesirablePlugin((*plugins)[j])) {
        // Skip the current undesirable one so we can use the better one
        // we just found.
        LOG_IF(ERROR, PluginList::DebugPluginLoading())
            << "Skipping " << info.path.value() << ", preferring "
            << (*plugins)[j].path.value();
        return false;
      }
    }
  }

  // TODO(evanm): prefer the newest version of flash, etc. here?

  VLOG_IF(1, PluginList::DebugPluginLoading()) << "Using " << info.path.value();

  return true;
}

}  // namespace npapi
}  // namespace webkit
