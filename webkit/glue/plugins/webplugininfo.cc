// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webplugininfo.h"

WebPluginMimeType::WebPluginMimeType() {}

WebPluginMimeType::~WebPluginMimeType() {}

WebPluginInfo::WebPluginInfo() : enabled(false) {}

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
                             const string16& fake_version,
                             const string16& fake_desc)
    : name(fake_name),
      path(),
      version(fake_version),
      desc(fake_desc),
      mime_types(),
      enabled(true) {
}

