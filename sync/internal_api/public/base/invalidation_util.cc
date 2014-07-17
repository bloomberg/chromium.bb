// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation_util.h"

#include <ostream>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "sync/internal_api/public/base/invalidation.h"

namespace invalidation {
void PrintTo(const invalidation::ObjectId& id, std::ostream* os) {
  *os << syncer::ObjectIdToString(id);
}
}  // namespace invalidation

namespace syncer {

bool ObjectIdLessThan::operator()(const invalidation::ObjectId& lhs,
                                  const invalidation::ObjectId& rhs) const {
  return (lhs.source() < rhs.source()) ||
         (lhs.source() == rhs.source() && lhs.name() < rhs.name());
}

bool InvalidationVersionLessThan::operator()(
    const Invalidation& a,
    const Invalidation& b) const {
  DCHECK(a.object_id() == b.object_id())
      << "a: " << ObjectIdToString(a.object_id()) << ", "
      << "b: " << ObjectIdToString(a.object_id());

  if (a.is_unknown_version() && !b.is_unknown_version())
    return true;

  if (!a.is_unknown_version() && b.is_unknown_version())
    return false;

  if (a.is_unknown_version() && b.is_unknown_version())
    return false;

  return a.version() < b.version();
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

scoped_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("source", object_id.source());
  value->SetString("name", object_id.name());
  return value.Pass();
}

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out) {
  *out = invalidation::ObjectId();
  std::string name;
  int source = 0;
  if (!value.GetInteger("source", &source) ||
      !value.GetString("name", &name)) {
    return false;
  }
  *out = invalidation::ObjectId(source, name);
  return true;
}

std::string ObjectIdToString(
    const invalidation::ObjectId& object_id) {
  scoped_ptr<base::DictionaryValue> value(ObjectIdToValue(object_id));
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

ObjectIdSet ModelTypeSetToObjectIdSet(ModelTypeSet model_types) {
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
