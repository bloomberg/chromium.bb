// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/object_id_state_map.h"

namespace syncer {

ObjectIdSet ObjectIdStateMapToSet(const ObjectIdStateMap& id_state_map) {
  ObjectIdSet ids;
  for (ObjectIdStateMap::const_iterator it = id_state_map.begin();
       it != id_state_map.end(); ++it) {
    ids.insert(it->first);
  }
  return ids;
}

ObjectIdStateMap ObjectIdSetToStateMap(const ObjectIdSet& ids,
                                       const std::string& payload) {
  ObjectIdStateMap id_state_map;
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    // TODO(dcheng): Do we need to provide a way to set AckHandle?
    id_state_map[*it].payload = payload;
  }
  return id_state_map;
}

ModelTypeStateMap ObjectIdStateMapToModelTypeStateMap(
    const ObjectIdStateMap& id_state_map) {
  ModelTypeStateMap type_state_map;
  for (ObjectIdStateMap::const_iterator it = id_state_map.begin();
       it != id_state_map.end(); ++it) {
    ModelType model_type;
    if (!ObjectIdToRealModelType(it->first, &model_type)) {
      DLOG(WARNING) << "Invalid object ID: "
                    << ObjectIdToString(it->first);
      continue;
    }
    type_state_map[model_type] = it->second;
  }
  return type_state_map;
}

ObjectIdStateMap ModelTypeStateMapToObjectIdStateMap(
    const ModelTypeStateMap& type_state_map) {
  ObjectIdStateMap id_state_map;
  for (ModelTypeStateMap::const_iterator it = type_state_map.begin();
       it != type_state_map.end(); ++it) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it->first, &id)) {
      DLOG(WARNING) << "Invalid model type " << it->first;
      continue;
    }
    id_state_map[id] = it->second;
  }
  return id_state_map;
}

}  // namespace syncer
