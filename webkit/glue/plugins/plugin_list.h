// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_
#define WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_

// This file is here to keep NativeClient compiling. PluginList was moved to
// webkit/plugins/npapi and into the webkit::npapi namespace. Native Client
// depends on this old location & namespace, so we provide just enough
// definitions here to keep it compiling until it can be updated to use the
// new location & namespace.
//
// TODO(brettw) remove this flie when NaCl is updated.

#include "webkit/plugins/npapi/plugin_list.h"

namespace NPAPI {

typedef webkit::npapi::PluginEntryPoints PluginEntryPoints;

class PluginList {
 public:
  static inline webkit::npapi::PluginList* Singleton() {
    return webkit::npapi::PluginList::Singleton();
  }
};

}  // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGINS_PLUGIN_LIST_H_
