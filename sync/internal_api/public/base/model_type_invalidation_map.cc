// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_invalidation_map.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace syncer {

ModelTypeInvalidationMap ModelTypeSetToInvalidationMap(
    ModelTypeSet types, const std::string& payload) {
  ModelTypeInvalidationMap invalidation_map;
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    // TODO(dcheng): Do we need to set ack_handle?
    invalidation_map[it.Get()].payload = payload;
  }
  return invalidation_map;
}

ModelTypeSet ModelTypeInvalidationMapToSet(
    const ModelTypeInvalidationMap& invalidation_map) {
  ModelTypeSet types;
  for (ModelTypeInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    types.Put(it->first);
  }
  return types;
}

std::string ModelTypeInvalidationMapToString(
    const ModelTypeInvalidationMap& invalidation_map) {
  scoped_ptr<DictionaryValue> value(
      ModelTypeInvalidationMapToValue(invalidation_map));
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  return json;
}

DictionaryValue* ModelTypeInvalidationMapToValue(
    const ModelTypeInvalidationMap& invalidation_map) {
  DictionaryValue* value = new DictionaryValue();
  for (ModelTypeInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    std::string printable_payload;
    base::JsonDoubleQuote(it->second.payload,
                          false /* put_in_quotes */,
                          &printable_payload);
    value->SetString(ModelTypeToString(it->first), printable_payload);
  }
  return value;
}

void CoalesceStates(const ModelTypeInvalidationMap& update,
                    ModelTypeInvalidationMap* original) {
  // TODO(dcheng): Where is this called? Do we need to add more clever logic for
  // handling ack_handle? We probably want to always use the "latest"
  // ack_handle, which might imply always using the one in update?
  for (ModelTypeInvalidationMap::const_iterator i = update.begin();
       i != update.end(); ++i) {
    if (original->count(i->first) == 0) {
      // If this datatype isn't already in our map, add it with
      // whatever payload it has.
      (*original)[i->first] = i->second;
    } else if (i->second.payload.length() > 0) {
      // If this datatype is already in our map, we only overwrite the
      // payload if the new one is non-empty.
      (*original)[i->first].payload = i->second.payload;
    }
  }
}

}  // namespace syncer
