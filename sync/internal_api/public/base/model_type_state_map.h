// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Definition of ModelTypeStateMap and various utility functions.

#ifndef SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_STATE_MAP_H_
#define SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_STATE_MAP_H_

#include <map>
#include <string>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation_state.h"
#include "sync/internal_api/public/base/model_type.h"

// TODO(akalin): Move the non-exported functions in this file to a
// private header.

namespace base {
class DictionaryValue;
}

namespace syncer {

// A container that contains a set of datatypes with possible string
// payloads.
typedef std::map<ModelType, InvalidationState> ModelTypeStateMap;

// Helper functions for building ModelTypeStateMaps.

// Make a TypeStateMap from all the types in a ModelTypeSet using a
// default payload.
SYNC_EXPORT ModelTypeStateMap ModelTypeSetToStateMap(
    ModelTypeSet model_types, const std::string& payload);

ModelTypeSet ModelTypeStateMapToSet(
    const ModelTypeStateMap& type_state_map);

std::string ModelTypeStateMapToString(
    const ModelTypeStateMap& type_state_map);

// Caller takes ownership of the returned dictionary.
base::DictionaryValue* ModelTypeStateMapToValue(
    const ModelTypeStateMap& type_state_map);

// Coalesce |update| into |original|, overwriting only when |update| has
// a non-empty payload.
void CoalesceStates(ModelTypeStateMap* original,
                    const ModelTypeStateMap& update);

}  // namespace syncer

#endif  // SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_STATE_MAP_H_
