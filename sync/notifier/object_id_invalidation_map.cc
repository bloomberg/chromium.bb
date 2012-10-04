// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/object_id_invalidation_map.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/values.h"

namespace syncer {

ObjectIdSet ObjectIdInvalidationMapToSet(
    const ObjectIdInvalidationMap& invalidation_map) {
  ObjectIdSet ids;
  for (ObjectIdInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    ids.insert(it->first);
  }
  return ids;
}

ObjectIdInvalidationMap ObjectIdSetToInvalidationMap(
    const ObjectIdSet& ids, const std::string& payload) {
  ObjectIdInvalidationMap invalidation_map;
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    // TODO(dcheng): Do we need to provide a way to set AckHandle?
    invalidation_map[*it].payload = payload;
  }
  return invalidation_map;
}

namespace {

struct ObjectIdInvalidationMapValueEquals {
  bool operator()(const ObjectIdInvalidationMap::value_type& value1,
                  const ObjectIdInvalidationMap::value_type& value2) const {
    return
        (value1.first == value2.first) &&
        value1.second.Equals(value2.second);
  }
};

}  // namespace

bool ObjectIdInvalidationMapEquals(
    const ObjectIdInvalidationMap& invalidation_map1,
    const ObjectIdInvalidationMap& invalidation_map2) {
  return
      (invalidation_map1.size() == invalidation_map2.size()) &&
      std::equal(invalidation_map1.begin(), invalidation_map1.end(),
                 invalidation_map2.begin(),
                 ObjectIdInvalidationMapValueEquals());
}

scoped_ptr<base::ListValue> ObjectIdInvalidationMapToValue(
    const ObjectIdInvalidationMap& invalidation_map) {
  scoped_ptr<ListValue> value(new ListValue());
  for (ObjectIdInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    DictionaryValue* entry = new DictionaryValue();
    entry->Set("objectId", ObjectIdToValue(it->first).release());
    entry->Set("state", it->second.ToValue().release());
    value->Append(entry);
  }
  return value.Pass();
}

bool ObjectIdInvalidationMapFromValue(const base::ListValue& value,
                               ObjectIdInvalidationMap* out) {
  out->clear();
  for (base::ListValue::const_iterator it = value.begin();
       it != value.end(); ++it) {
    const base::DictionaryValue* entry = NULL;
    const base::DictionaryValue* id_value = NULL;
    const base::DictionaryValue* invalidation_value = NULL;
    invalidation::ObjectId id;
    Invalidation invalidation;
    if (!(*it)->GetAsDictionary(&entry) ||
        !entry->GetDictionary("objectId", &id_value) ||
        !entry->GetDictionary("state", &invalidation_value) ||
        !ObjectIdFromValue(*id_value, &id) ||
        !invalidation.ResetFromValue(*invalidation_value)) {
      return false;
    }
    ignore_result(out->insert(std::make_pair(id, invalidation)));
  }
  return true;
}

ModelTypeInvalidationMap ObjectIdInvalidationMapToModelTypeInvalidationMap(
    const ObjectIdInvalidationMap& invalidation_map) {
  ModelTypeInvalidationMap type_invalidation_map;
  for (ObjectIdInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    ModelType model_type;
    if (!ObjectIdToRealModelType(it->first, &model_type)) {
      DLOG(WARNING) << "Invalid object ID: "
                    << ObjectIdToString(it->first);
      continue;
    }
    type_invalidation_map[model_type] = it->second;
  }
  return type_invalidation_map;
}

ObjectIdInvalidationMap ModelTypeInvalidationMapToObjectIdInvalidationMap(
    const ModelTypeInvalidationMap& invalidation_map) {
  ObjectIdInvalidationMap id_invalidation_map;
  for (ModelTypeInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    invalidation::ObjectId id;
    if (!RealModelTypeToObjectId(it->first, &id)) {
      DLOG(WARNING) << "Invalid model type " << it->first;
      continue;
    }
    id_invalidation_map[id] = it->second;
  }
  return id_invalidation_map;
}

}  // namespace syncer
