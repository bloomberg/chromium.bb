// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/mock_plugin_list.h"

namespace webkit {
namespace npapi {

MockPluginList::MockPluginList(const PluginGroupDefinition* group_definitions,
                               size_t num_group_definitions) :
    PluginList(group_definitions, num_group_definitions) {
}

MockPluginList::~MockPluginList() {
}

void MockPluginList::AddPluginToLoad(const WebPluginInfo& plugin) {
  plugins_to_load_.push_back(plugin);
}

void MockPluginList::ClearPluginsToLoad() {
  plugins_to_load_.clear();
}

bool MockPluginList::GetPluginsNoRefresh(
      std::vector<webkit::WebPluginInfo>* plugins) {
  GetPlugins(plugins);
  return true;
}

void MockPluginList::LoadPluginsInternal(
    ScopedVector<PluginGroup>* plugin_groups) {
  for (size_t i = 0; i < plugins_to_load_.size(); ++i)
    AddToPluginGroups(plugins_to_load_[i], plugin_groups);
}

}  // npapi
}  // webkit
