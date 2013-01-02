// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Definition of ModelTypeInvalidationMap and various utility functions.

#ifndef SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_INVALIDATION_MAP_H_
#define SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_INVALIDATION_MAP_H_

#include <map>
#include <string>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/internal_api/public/base/model_type.h"

// TODO(akalin): Move the non-exported functions in this file to a
// private header.

namespace base {
class DictionaryValue;
}

namespace syncer {

// A map between sync data types and their associated invalidation.
typedef std::map<ModelType, Invalidation> ModelTypeInvalidationMap;

// Helper functions for building ModelTypeInvalidationMaps.

// Make a ModelTypeInvalidationMap from all the types in a ModelTypeSet using a
// default payload.
SYNC_EXPORT ModelTypeInvalidationMap ModelTypeSetToInvalidationMap(
    ModelTypeSet model_types, const std::string& payload);

SYNC_EXPORT_PRIVATE ModelTypeSet ModelTypeInvalidationMapToSet(
    const ModelTypeInvalidationMap& invalidation_map);

std::string ModelTypeInvalidationMapToString(
    const ModelTypeInvalidationMap& invalidation_map);

// Caller takes ownership of the returned dictionary.
SYNC_EXPORT_PRIVATE base::DictionaryValue* ModelTypeInvalidationMapToValue(
    const ModelTypeInvalidationMap& invalidation_map);

// Coalesce |update| into |original|, overwriting only when |update| has
// a non-empty payload.
SYNC_EXPORT_PRIVATE void CoalesceStates(ModelTypeInvalidationMap* original,
                                        const ModelTypeInvalidationMap& update);

}  // namespace syncer

#endif  // SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_INVALIDATION_MAP_H_
