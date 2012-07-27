// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/object_id_payload_map.h"

namespace syncer {

ModelTypePayloadMap ObjectIdPayloadMapToModelTypePayloadMap(
    const ObjectIdPayloadMap& id_payloads) {
  ModelTypePayloadMap types_with_payloads;
  for (ObjectIdPayloadMap::const_iterator it = id_payloads.begin();
       it != id_payloads.end(); ++it) {
    ModelType model_type;
    if (!ObjectIdToRealModelType(it->first, &model_type)) {
      DLOG(WARNING) << "Invalid object ID: "
                    << ObjectIdToString(it->first);
      continue;
    }
    types_with_payloads[model_type] = it->second;
  }
  return types_with_payloads;
}

ObjectIdPayloadMap ModelTypePayloadMapToObjectIdPayloadMap(
    const ModelTypePayloadMap& type_payloads) {
  ObjectIdPayloadMap id_payloads;
  for (ModelTypePayloadMap::const_iterator it = type_payloads.begin();
       it != type_payloads.end(); ++it) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it->first, &id)) {
      DLOG(WARNING) << "Invalid model type " << it->first;
      continue;
    }
    id_payloads[id] = it->second;
  }
  return id_payloads;
}

}  // namespace syncer
