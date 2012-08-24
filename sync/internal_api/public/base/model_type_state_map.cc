// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_state_map.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace syncer {

ModelTypeStateMap ModelTypeSetToStateMap(ModelTypeSet types,
                                         const std::string& payload) {
  ModelTypeStateMap type_state_map;
  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    // TODO(dcheng): Do we need to set ack_handle?
    type_state_map[it.Get()].payload = payload;
  }
  return type_state_map;
}

ModelTypeSet ModelTypeStateMapToSet(
    const ModelTypeStateMap& type_state_map) {
  ModelTypeSet types;
  for (ModelTypeStateMap::const_iterator it = type_state_map.begin();
       it != type_state_map.end(); ++it) {
    types.Put(it->first);
  }
  return types;
}

std::string ModelTypeStateMapToString(const ModelTypeStateMap& type_state_map) {
  scoped_ptr<DictionaryValue> value(ModelTypeStateMapToValue(type_state_map));
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  return json;
}

DictionaryValue* ModelTypeStateMapToValue(
    const ModelTypeStateMap& type_state_map) {
  DictionaryValue* value = new DictionaryValue();
  for (ModelTypeStateMap::const_iterator it = type_state_map.begin();
       it != type_state_map.end(); ++it) {
    std::string printable_payload;
    base::JsonDoubleQuote(it->second.payload,
                          false /* put_in_quotes */,
                          &printable_payload);
    value->SetString(ModelTypeToString(it->first), printable_payload);
  }
  return value;
}

void CoalesceStates(ModelTypeStateMap* original,
                    const ModelTypeStateMap& update) {
  // TODO(dcheng): Where is this called? Do we need to add more clever logic for
  // handling ack_handle? We probably want to always use the "latest"
  // ack_handle, which might imply always using the one in update?
  for (ModelTypeStateMap::const_iterator i = update.begin();
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
