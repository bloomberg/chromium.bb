// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/object_id_state_map.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/values.h"

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

namespace {

struct ObjectIdStateMapValueEquals {
  bool operator()(const ObjectIdStateMap::value_type& value1,
                  const ObjectIdStateMap::value_type& value2) const {
    return
        (value1.first == value2.first) &&
        value1.second.Equals(value2.second);
  }
};

}  // namespace

bool ObjectIdStateMapEquals(const ObjectIdStateMap& id_state_map1,
                            const ObjectIdStateMap& id_state_map2) {
  return
      (id_state_map1.size() == id_state_map2.size()) &&
      std::equal(id_state_map1.begin(), id_state_map1.end(),
                 id_state_map2.begin(), ObjectIdStateMapValueEquals());
}

scoped_ptr<base::ListValue> ObjectIdStateMapToValue(
    const ObjectIdStateMap& id_state_map) {
  scoped_ptr<ListValue> value(new ListValue());
  for (ObjectIdStateMap::const_iterator it = id_state_map.begin();
       it != id_state_map.end(); ++it) {
    DictionaryValue* entry = new DictionaryValue();
    entry->Set("objectId", ObjectIdToValue(it->first).release());
    entry->Set("state", it->second.ToValue().release());
    value->Append(entry);
  }
  return value.Pass();
}

bool ObjectIdStateMapFromValue(const base::ListValue& value,
                               ObjectIdStateMap* out) {
  out->clear();
  for (base::ListValue::const_iterator it = value.begin();
       it != value.end(); ++it) {
    const base::DictionaryValue* entry = NULL;
    const base::DictionaryValue* id_value = NULL;
    const base::DictionaryValue* state_value = NULL;
    invalidation::ObjectId id;
    InvalidationState state;
    if (!(*it)->GetAsDictionary(&entry) ||
        !entry->GetDictionary("objectId", &id_value) ||
        !entry->GetDictionary("state", &state_value) ||
        !ObjectIdFromValue(*id_value, &id) ||
        !state.ResetFromValue(*state_value)) {
      return false;
    }
    ignore_result(out->insert(std::make_pair(id, state)));
  }
  return true;
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
