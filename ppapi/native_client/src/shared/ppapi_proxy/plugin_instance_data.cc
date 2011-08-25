/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"

#include <tr1/unordered_map>

namespace ppapi_proxy {

namespace {

typedef std::tr1::unordered_map<PP_Instance, PluginInstanceData*> InstanceMap;

InstanceMap& GetInstanceMap() {
  static InstanceMap map;
  return map;
}

}  // namespace

// static
PluginInstanceData* PluginInstanceData::FromPP(PP_Instance id) {
  InstanceMap& map = GetInstanceMap();
  InstanceMap::iterator i = map.find(id);

  return map.end() == i ? NULL : i->second;
}

// static
void PluginInstanceData::DidCreate(PP_Instance id) {
  InstanceMap& map = GetInstanceMap();
  // TODO(neb): figure out how to use CHECK in NaCl land.
//  CHECK(map.end() == map.find(id));
  map[id] = new PluginInstanceData(id);
}

// static
void PluginInstanceData::DidDestroy(PP_Instance id) {
  GetInstanceMap().erase(id);
}

// static
void PluginInstanceData::DidChangeView(PP_Instance id,
                                       PP_Rect position,
                                       PP_Rect clip) {
  PluginInstanceData* instance = FromPP(id);
  if (instance) {
    instance->set_position(position);
  }
}

}  // namespace ppapi_proxy
