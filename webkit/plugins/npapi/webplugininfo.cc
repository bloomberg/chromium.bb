// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/webplugininfo.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace webkit {
namespace npapi {

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
    : enabled(USER_DISABLED_POLICY_UNMANAGED) {
}

WebPluginInfo::WebPluginInfo(const WebPluginInfo& rhs)
    : name(rhs.name),
      path(rhs.path),
      version(rhs.version),
      desc(rhs.desc),
      mime_types(rhs.mime_types),
      enabled(rhs.enabled) {
}

WebPluginInfo::~WebPluginInfo() {}

WebPluginInfo& WebPluginInfo::operator=(const WebPluginInfo& rhs) {
  name = rhs.name;
  path = rhs.path;
  version = rhs.version;
  desc = rhs.desc;
  mime_types = rhs.mime_types;
  enabled = rhs.enabled;
  return *this;
}

WebPluginInfo::WebPluginInfo(const string16& fake_name,
                             const FilePath& fake_path,
                             const string16& fake_version,
                             const string16& fake_desc)
    : name(fake_name),
      path(fake_path),
      version(fake_version),
      desc(fake_desc),
      mime_types(),
      enabled(USER_ENABLED_POLICY_UNMANAGED) {
}

bool IsPluginEnabled(const WebPluginInfo& plugin) {
  return ((plugin.enabled & WebPluginInfo::POLICY_ENABLED) ||
          plugin.enabled == WebPluginInfo::USER_ENABLED_POLICY_UNMANAGED);
}

}  // namespace npapi
}  // namespace webkit
