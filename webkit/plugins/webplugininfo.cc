// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/webplugininfo.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace webkit {

WebPluginMimeType::WebPluginMimeType() {}

WebPluginMimeType::WebPluginMimeType(const std::string& m,
                                     const std::string& f,
                                     const std::string& d)
    : mime_type(m),
      file_extensions(),
      description(ASCIIToUTF16(d)) {
  file_extensions.push_back(f);
}

WebPluginMimeType::~WebPluginMimeType() {}

WebPluginInfo::WebPluginInfo()
    : type(PLUGIN_TYPE_NPAPI),
      pepper_permissions(0) {
}

WebPluginInfo::WebPluginInfo(const WebPluginInfo& rhs)
    : name(rhs.name),
      path(rhs.path),
      version(rhs.version),
      desc(rhs.desc),
      mime_types(rhs.mime_types),
      type(rhs.type),
      pepper_permissions(rhs.pepper_permissions) {
}

WebPluginInfo::~WebPluginInfo() {}

WebPluginInfo& WebPluginInfo::operator=(const WebPluginInfo& rhs) {
  name = rhs.name;
  path = rhs.path;
  version = rhs.version;
  desc = rhs.desc;
  mime_types = rhs.mime_types;
  type = rhs.type;
  pepper_permissions = rhs.pepper_permissions;
  return *this;
}

WebPluginInfo::WebPluginInfo(const base::string16& fake_name,
                             const base::FilePath& fake_path,
                             const base::string16& fake_version,
                             const base::string16& fake_desc)
    : name(fake_name),
      path(fake_path),
      version(fake_version),
      desc(fake_desc),
      mime_types(),
      type(PLUGIN_TYPE_NPAPI),
      pepper_permissions(0) {
}

bool IsPepperPlugin(const WebPluginInfo& plugin) {
  return ((plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS ) ||
          (plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) ||
          (plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED));
}

bool IsOutOfProcessPlugin(const WebPluginInfo& plugin) {
  return ((plugin.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) ||
          (plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) ||
          (plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED));
}

}  // namespace webkit
