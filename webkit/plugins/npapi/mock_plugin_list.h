// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_MOCK_PLUGIN_LIST_H_
#define WEBKIT_PLUGINS_NPAPI_MOCK_PLUGIN_LIST_H_

#include "webkit/plugins/npapi/plugin_list.h"

namespace webkit {
namespace npapi {

// A PluginList for tests that avoids file system IO. There is also no reason
// to use |lock_| (but it doesn't hurt either).
class MockPluginList : public PluginList {
 public:
  MockPluginList(const PluginGroupDefinition* group_definitions,
                 size_t num_group_definitions);
  virtual ~MockPluginList();

  void AddPluginToLoad(const WebPluginInfo& plugin);
  void ClearPluginsToLoad();

  // PluginList:
  virtual bool GetPluginsIfNoRefreshNeeded(
      std::vector<webkit::WebPluginInfo>* plugins) OVERRIDE;

 private:
  std::vector<WebPluginInfo> plugins_to_load_;

  // PluginList methods:
  virtual void LoadPluginsInternal(
      ScopedVector<PluginGroup>* plugin_groups) OVERRIDE;
};

}  // npapi
}  // webkit

#endif  // WEBKIT_PLUGINS_NPAPI_MOCK_PLUGIN_LIST_H_
