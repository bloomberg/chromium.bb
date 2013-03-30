// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_lib.h"

#include <dlfcn.h>
#if defined(OS_OPENBSD)
#include <sys/exec_elf.h>
#else
#include <elf.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "webkit/plugins/npapi/plugin_list.h"

// These headers must be included in this order to make the declaration gods
// happy.
#include "base/third_party/nspr/prcpucfg_linux.h"

namespace webkit {
namespace npapi {

namespace {

// Copied from nsplugindefs.h instead of including the file since it has a bunch
// of dependencies.
enum nsPluginVariable {
  nsPluginVariable_NameString        = 1,
  nsPluginVariable_DescriptionString = 2
};

// Read the ELF header and return true if it is usable on
// the current architecture (e.g. 32-bit ELF on 32-bit build).
// Returns false on other errors as well.
bool ELFMatchesCurrentArchitecture(const base::FilePath& filename) {
  // First make sure we can open the file and it is in fact, a regular file.
  struct stat stat_buf;
  // Open with O_NONBLOCK so we don't block on pipes.
  int fd = open(filename.value().c_str(), O_RDONLY|O_NONBLOCK);
  if (fd < 0)
    return false;
  bool ret = (fstat(fd, &stat_buf) >= 0 && S_ISREG(stat_buf.st_mode));
  if (HANDLE_EINTR(close(fd)) < 0)
    return false;
  if (!ret)
    return false;

  const size_t kELFBufferSize = 5;
  char buffer[kELFBufferSize];
  if (!file_util::ReadFile(filename, buffer, kELFBufferSize))
    return false;

  if (buffer[0] != ELFMAG0 ||
      buffer[1] != ELFMAG1 ||
      buffer[2] != ELFMAG2 ||
      buffer[3] != ELFMAG3) {
    // Not an ELF file, perhaps?
    return false;
  }

  int elf_class = buffer[EI_CLASS];
#if defined(ARCH_CPU_32_BITS)
  if (elf_class == ELFCLASS32)
    return true;
#elif defined(ARCH_CPU_64_BITS)
  if (elf_class == ELFCLASS64)
    return true;
#endif

  return false;
}

// This structure matches enough of nspluginwrapper's NPW_PluginInfo
// for us to extract the real plugin path.
struct __attribute__((packed)) NSPluginWrapperInfo {
  char ident[32];  // NSPluginWrapper magic identifier (includes version).
  char path[PATH_MAX];  // Path to wrapped plugin.
};

// Test a plugin for whether it's been wrapped by NSPluginWrapper, and
// if so attempt to unwrap it.  Pass in an opened plugin handle; on
// success, |dl| and |unwrapped_path| will be filled in with the newly
// opened plugin.  On failure, params are left unmodified.
void UnwrapNSPluginWrapper(void **dl, base::FilePath* unwrapped_path) {
  NSPluginWrapperInfo* info =
      reinterpret_cast<NSPluginWrapperInfo*>(dlsym(*dl, "NPW_Plugin"));
  if (!info)
    return;  // Not a NSPW plugin.

  // Here we could check the NSPW ident field for the versioning
  // information, but the path field is available in all versions
  // anyway.

  // Grab the path to the wrapped plugin.  Just in case the structure
  // format changes, protect against the path not being null-terminated.
  char* path_end = static_cast<char*>(memchr(info->path, '\0',
                                             sizeof(info->path)));
  if (!path_end)
    path_end = info->path + sizeof(info->path);
  base::FilePath path = base::FilePath(
      std::string(info->path, path_end - info->path));

  if (!ELFMatchesCurrentArchitecture(path)) {
    LOG(WARNING) << path.value() << " is nspluginwrapper wrapping a "
                 << "plugin for a different architecture; it will "
                 << "work better if you instead use a native plugin.";
    return;
  }

  std::string error;
  void* newdl = base::LoadNativeLibrary(path, &error);
  if (!newdl) {
    // We couldn't load the unwrapped plugin for some reason, despite
    // being able to load the wrapped one.  Just use the wrapped one.
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Could not use unwrapped nspluginwrapper plugin "
        << unwrapped_path->value() << " (" << error << "), "
        << "using the wrapped one.";
    return;
  }

  // Unload the wrapped plugin, and use the wrapped plugin instead.
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Using unwrapped version " << unwrapped_path->value()
      << " of nspluginwrapper-wrapped plugin.";
  base::UnloadNativeLibrary(*dl);
  *dl = newdl;
  *unwrapped_path = path;
}

}  // namespace

bool PluginLib::ReadWebPluginInfo(const base::FilePath& filename,
                                  WebPluginInfo* info) {
  // The file to reference is:
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUnix.cpp

  // Skip files that aren't appropriate for our architecture.
  if (!ELFMatchesCurrentArchitecture(filename)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Skipping plugin " << filename.value()
        << " because it doesn't match the current architecture.";
    return false;
  }

  std::string error;
  void* dl = base::LoadNativeLibrary(filename, &error);
  if (!dl) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "While reading plugin info, unable to load library "
        << filename.value() << " (" << error << "), skipping.";
    return false;
  }

  info->path = filename;

  // Attempt to swap in the wrapped plugin if this is nspluginwrapper.
  UnwrapNSPluginWrapper(&dl, &info->path);

  // See comments in plugin_lib_mac regarding this symbol.
  typedef const char* (*NP_GetMimeDescriptionType)();
  NP_GetMimeDescriptionType NP_GetMIMEDescription =
      reinterpret_cast<NP_GetMimeDescriptionType>(
          dlsym(dl, "NP_GetMIMEDescription"));
  const char* mime_description = NULL;
  if (!NP_GetMIMEDescription) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << filename.value() << " doesn't have a "
        << "NP_GetMIMEDescription symbol";
    return false;
  }
  mime_description = NP_GetMIMEDescription();

  if (!mime_description) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "MIME description for " << filename.value() << " is empty";
    return false;
  }
  ParseMIMEDescription(mime_description, &info->mime_types);

  // The plugin name and description live behind NP_GetValue calls.
  typedef NPError (*NP_GetValueType)(void* unused,
                                     nsPluginVariable variable,
                                     void* value_out);
  NP_GetValueType NP_GetValue =
      reinterpret_cast<NP_GetValueType>(dlsym(dl, "NP_GetValue"));
  if (NP_GetValue) {
    const char* name = NULL;
    NP_GetValue(NULL, nsPluginVariable_NameString, &name);
    if (name) {
      info->name = UTF8ToUTF16(name);
      ExtractVersionString(name, info);
    }

    const char* description = NULL;
    NP_GetValue(NULL, nsPluginVariable_DescriptionString, &description);
    if (description) {
      info->desc = UTF8ToUTF16(description);
      if (info->version.empty())
        ExtractVersionString(description, info);
    }

    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Got info for plugin " << filename.value()
        << " Name = \"" << UTF16ToUTF8(info->name)
        << "\", Description = \"" << UTF16ToUTF8(info->desc)
        << "\", Version = \"" << UTF16ToUTF8(info->version)
        << "\".";
  } else {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << filename.value()
        << " has no GetValue() and probably won't work.";
  }

  // Intentionally not unloading the plugin here, it can lead to crashes.

  return true;
}

// static
void PluginLib::ParseMIMEDescription(
    const std::string& description,
    std::vector<WebPluginMimeType>* mime_types) {
  // We parse the description here into WebPluginMimeType structures.
  // Naively from the NPAPI docs you'd think you could use
  // string-splitting, but the Firefox parser turns out to do something
  // different: find the first colon, then the second, then a semi.
  //
  // See ParsePluginMimeDescription near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUtils.h#53

  std::string::size_type ofs = 0;
  for (;;) {
    WebPluginMimeType mime_type;

    std::string::size_type end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    mime_type.mime_type = description.substr(ofs, end - ofs);
    ofs = end + 1;

    end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    const std::string extensions = description.substr(ofs, end - ofs);
    base::SplitString(extensions, ',', &mime_type.file_extensions);
    ofs = end + 1;

    end = description.find(';', ofs);
    // It's ok for end to run off the string here.  If there's no
    // trailing semicolon we consume the remainder of the string.
    if (end != std::string::npos) {
      mime_type.description = UTF8ToUTF16(description.substr(ofs, end - ofs));
    } else {
      mime_type.description = UTF8ToUTF16(description.substr(ofs));
    }
    mime_types->push_back(mime_type);
    if (end == std::string::npos)
      break;
    ofs = end + 1;
  }
}

// static
void PluginLib::ExtractVersionString(const std::string& desc,
                                     WebPluginInfo* info) {
  // This matching works by extracting a version substring, along the lines of:
  // No postfix: second match in .*<prefix>.*$
  // With postfix: second match .*<prefix>.*<postfix>
  static const struct {
    const char* kPrefix;
    const char* kPostfix;
  } kPrePostFixes[] = {
    { "Shockwave Flash ", 0 },
    { "Java(TM) Plug-in ", 0 },
    { "(using IcedTea-Web ", " " },
    { 0, 0 }
  };
  std::string version;
  for (size_t i = 0; kPrePostFixes[i].kPrefix; ++i) {
    size_t pos;
    if ((pos = desc.find(kPrePostFixes[i].kPrefix)) != std::string::npos) {
      version = desc.substr(pos + strlen(kPrePostFixes[i].kPrefix));
      pos = std::string::npos;
      if (kPrePostFixes[i].kPostfix)
        pos = version.find(kPrePostFixes[i].kPostfix);
      if (pos != std::string::npos)
        version = version.substr(0, pos);
      break;
    }
  }
  if (!version.empty()) {
    info->version = UTF8ToUTF16(version);
  }
}

}  // namespace npapi
}  // namespace webkit
