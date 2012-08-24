// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#define SYNC_NOTIFIER_INVALIDATION_UTIL_H_

#include <iosfwd>
#include <set>
#include <string>

#include "sync/internal_api/public/base/model_type.h"

namespace invalidation {

class Invalidation;
class ObjectId;

// Gmock print helper
void PrintTo(const invalidation::ObjectId& id, std::ostream* os);

}  // namespace invalidation

namespace syncer {

struct ObjectIdLessThan {
  bool operator()(const invalidation::ObjectId& lhs,
                  const invalidation::ObjectId& rhs) const;
};

typedef std::set<invalidation::ObjectId, ObjectIdLessThan> ObjectIdSet;

bool RealModelTypeToObjectId(ModelType model_type,
                             invalidation::ObjectId* object_id);

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             ModelType* model_type);

ObjectIdSet ModelTypeSetToObjectIdSet(const ModelTypeSet& models);
ModelTypeSet ObjectIdSetToModelTypeSet(const ObjectIdSet& ids);

std::string ObjectIdToString(const invalidation::ObjectId& object_id);

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_UTIL_H_
