// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_OBJECT_ID_PAYLOAD_MAP_H_
#define SYNC_NOTIFIER_OBJECT_ID_PAYLOAD_MAP_H_

#include <map>
#include <string>

#include "google/cacheinvalidation/include/types.h"
#include "sync/internal_api/public/base/model_type_payload_map.h"
#include "sync/notifier/invalidation_util.h"

namespace syncer {

typedef std::map<invalidation::ObjectId,
                 std::string,
                 ObjectIdLessThan> ObjectIdPayloadMap;

// Converts between ObjectIdPayloadMaps and ModelTypePayloadMaps.
ModelTypePayloadMap ObjectIdPayloadMapToModelTypePayloadMap(
    const ObjectIdPayloadMap& id_payloads);
ObjectIdPayloadMap ModelTypePayloadMapToObjectIdPayloadMap(
    const ModelTypePayloadMap& type_payloads);

}  // namespace syncer

#endif  // HOME_DCHENG_SRC_CHROMIUM_SRC_SYNC_NOTIFIER_OBJECT_ID_PAYLOAD_MAP_H_
