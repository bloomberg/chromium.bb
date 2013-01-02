// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#define SYNC_NOTIFIER_INVALIDATION_UTIL_H_

#include <iosfwd>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace base {
class DictionaryValue;
}  // namespace

namespace invalidation {

class Invalidation;
class ObjectId;

// Gmock print helper
SYNC_EXPORT_PRIVATE void PrintTo(const invalidation::ObjectId& id,
                                 std::ostream* os);

}  // namespace invalidation

namespace syncer {

struct SYNC_EXPORT ObjectIdLessThan {
  bool operator()(const invalidation::ObjectId& lhs,
                  const invalidation::ObjectId& rhs) const;
};

typedef std::set<invalidation::ObjectId, ObjectIdLessThan> ObjectIdSet;

SYNC_EXPORT bool RealModelTypeToObjectId(ModelType model_type,
                                         invalidation::ObjectId* object_id);

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             ModelType* model_type);

// Caller owns the returned DictionaryValue.
scoped_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id);

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out);

SYNC_EXPORT_PRIVATE std::string ObjectIdToString(
    const invalidation::ObjectId& object_id);

SYNC_EXPORT_PRIVATE ObjectIdSet ModelTypeSetToObjectIdSet(ModelTypeSet models);
ModelTypeSet ObjectIdSetToModelTypeSet(const ObjectIdSet& ids);

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_INVALIDATION_UTIL_H_
