// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_OBJECT_ID_INVALIDATION_MAP_H_
#define SYNC_NOTIFIER_OBJECT_ID_INVALIDATION_MAP_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/notifier/invalidation_util.h"

namespace base {
class ListValue;
}  // namespace base

namespace syncer {

typedef std::map<invalidation::ObjectId,
                 Invalidation,
                 ObjectIdLessThan> ObjectIdInvalidationMap;

// Converts between ObjectIdInvalidationMaps and ObjectIdSets.
ObjectIdSet ObjectIdInvalidationMapToSet(
    const ObjectIdInvalidationMap& invalidation_map);
SYNC_EXPORT ObjectIdInvalidationMap ObjectIdSetToInvalidationMap(
    const ObjectIdSet& ids, const std::string& payload);

SYNC_EXPORT bool ObjectIdInvalidationMapEquals(
    const ObjectIdInvalidationMap& invalidation_map1,
    const ObjectIdInvalidationMap& invalidation_map2);

scoped_ptr<base::ListValue> ObjectIdInvalidationMapToValue(
    const ObjectIdInvalidationMap& invalidation_map);

bool ObjectIdInvalidationMapFromValue(const base::ListValue& value,
                               ObjectIdInvalidationMap* out);

// Converts between ObjectIdInvalidationMaps and ModelTypeInvalidationMaps.
SYNC_EXPORT_PRIVATE ModelTypeInvalidationMap
    ObjectIdInvalidationMapToModelTypeInvalidationMap(
        const ObjectIdInvalidationMap& invalidation_map);
SYNC_EXPORT ObjectIdInvalidationMap
    ModelTypeInvalidationMapToObjectIdInvalidationMap(
        const ModelTypeInvalidationMap& invalidation_map);

}  // namespace syncer

#endif  // HOME_DCHENG_SRC_CHROMIUM_SRC_SYNC_NOTIFIER_OBJECT_ID_STATE_MAP_H_
