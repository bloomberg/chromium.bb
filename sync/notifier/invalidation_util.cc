// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_util.h"

#include <sstream>

#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/v2/types.pb.h"

namespace syncer {

bool ObjectIdLessThan::operator()(const invalidation::ObjectId& lhs,
                                  const invalidation::ObjectId& rhs) const {
  return (lhs.source() < rhs.source()) ||
         (lhs.source() == rhs.source() && lhs.name() < rhs.name());
}

bool RealModelTypeToObjectId(ModelType model_type,
                             invalidation::ObjectId* object_id) {
  std::string notification_type;
  if (!RealModelTypeToNotificationType(model_type, &notification_type)) {
    return false;
  }
  object_id->Init(ipc::invalidation::ObjectSource::CHROME_SYNC,
                  notification_type);
  return true;
}

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             ModelType* model_type) {
  return NotificationTypeToRealModelType(object_id.name(), model_type);
}

ObjectIdSet ModelTypeSetToObjectIdSet(const ModelTypeSet& model_types) {
  ObjectIdSet ids;
  for (ModelTypeSet::Iterator it = model_types.First(); it.Good(); it.Inc()) {
    invalidation::ObjectId model_type_as_id;
    if (!RealModelTypeToObjectId(it.Get(), &model_type_as_id)) {
      DLOG(WARNING) << "Invalid model type " << it.Get();
      continue;
    }
    ids.insert(model_type_as_id);
  }
  return ids;
}

ModelTypeSet ObjectIdSetToModelTypeSet(const ObjectIdSet& ids) {
  ModelTypeSet model_types;
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    ModelType model_type;
    if (!ObjectIdToRealModelType(*it, &model_type)) {
      DLOG(WARNING) << "Invalid object ID " << ObjectIdToString(*it);
      continue;
    }
    model_types.Put(model_type);
  }
  return model_types;
}

std::string ObjectIdToString(
    const invalidation::ObjectId& object_id) {
  std::stringstream ss;
  ss << "{ ";
  ss << "name: " << object_id.name() << ", ";
  ss << "source: " << object_id.source();
  ss << " }";
  return ss.str();
}

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation) {
  std::stringstream ss;
  ss << "{ ";
  ss << "object_id: " << ObjectIdToString(invalidation.object_id()) << ", ";
  ss << "version: " << invalidation.version();
  ss << " }";
  return ss.str();
}

}  // namespace syncer
