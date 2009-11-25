// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_lib.h"

#include <dlfcn.h>
#include <elf.h>

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"

// These headers must be included in this order to make the declaration gods
// happy.
#include "base/third_party/nspr/prcpucfg_linux.h"

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
bool ELFMatchesCurrentArchitecture(const FilePath& filename) {
  FILE* file = fopen(filename.value().c_str(), "rb");
  if (!file)
    return false;

  char buffer[5];
  if (fread(buffer, 5, 1, file) != 1) {
    fclose(file);
    return false;
  }
  fclose(file);

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

// TODO(thestig) This is a hack to work around the crash in bug 25245. Remove
// this once we read plugins out of process.
bool SkipPluginUnloadHack(const WebPluginInfo& info) {
  std::string filename = info.path.BaseName().value();
  return (filename.find("npo3dautoplugin") != std::string::npos);  // O3D
}

}  // anonymous namespace
namespace NPAPI {

bool PluginLib::ReadWebPluginInfo(const FilePath& filename,
                                  WebPluginInfo* info) {
  // The file to reference is:
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUnix.cpp

  // Skip files that aren't appropriate for our architecture.
  if (!ELFMatchesCurrentArchitecture(filename))
    return false;

  void* dl = base::LoadNativeLibrary(filename);
  if (!dl)
    return false;

  info->path = filename;

  // See comments in plugin_lib_mac regarding this symbol.
  typedef const char* (*NP_GetMimeDescriptionType)();
  NP_GetMimeDescriptionType NP_GetMIMEDescription =
      reinterpret_cast<NP_GetMimeDescriptionType>(
          dlsym(dl, "NP_GetMIMEDescription"));
  const char* mime_description = NULL;
  if (NP_GetMIMEDescription)
    mime_description = NP_GetMIMEDescription();

  if (mime_description)
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
    if (name)
      info->name = UTF8ToWide(name);

    const char* description = NULL;
    NP_GetValue(NULL, nsPluginVariable_DescriptionString, &description);
    if (description)
      info->desc = UTF8ToWide(description);
  }

  if (!SkipPluginUnloadHack(*info))
    base::UnloadNativeLibrary(dl);

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
    SplitString(extensions, ',', &mime_type.file_extensions);
    ofs = end + 1;

    end = description.find(';', ofs);
    // It's ok for end to run off the string here.  If there's no
    // trailing semicolon we consume the remainder of the string.
    if (end != std::string::npos) {
      mime_type.description = UTF8ToWide(description.substr(ofs, end - ofs));
    } else {
      mime_type.description = UTF8ToWide(description.substr(ofs));
    }
    mime_types->push_back(mime_type);
    if (end == std::string::npos)
      break;
    ofs = end + 1;
  }
}

}  // namespace NPAPI
