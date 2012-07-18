// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Definition of ModelTypePayloadMap and various utility functions.

#ifndef SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_PAYLOAD_MAP_H_
#define SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_PAYLOAD_MAP_H_

#include <map>
#include <string>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

// TODO(akalin): Move the non-exported functions in this file to a
// private header.

namespace base {
class DictionaryValue;
}

namespace syncer {

// A container that contains a set of datatypes with possible string
// payloads.
typedef std::map<ModelType, std::string> ModelTypePayloadMap;

// Helper functions for building ModelTypePayloadMaps.

// Make a TypePayloadMap from all the types in a ModelTypeSet using a
// default payload.
SYNC_EXPORT ModelTypePayloadMap ModelTypePayloadMapFromEnumSet(
    ModelTypeSet model_types, const std::string& payload);

ModelTypeSet ModelTypePayloadMapToEnumSet(
    const ModelTypePayloadMap& payload_map);

std::string ModelTypePayloadMapToString(
    const ModelTypePayloadMap& model_type_payloads);

// Caller takes ownership of the returned dictionary.
base::DictionaryValue* ModelTypePayloadMapToValue(
    const ModelTypePayloadMap& model_type_payloads);

// Coalesce |update| into |original|, overwriting only when |update| has
// a non-empty payload.
void CoalescePayloads(ModelTypePayloadMap* original,
                      const ModelTypePayloadMap& update);

}  // namespace syncer

#endif  // SYNC_INTERNAL_PUBLIC_API_BASE_MODEL_TYPE_PAYLOAD_MAP_H_
