// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#define SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#pragma once

#include <string>

#include "google/cacheinvalidation/deps/callback.h"
#include "sync/internal_api/public/syncable/model_type.h"

namespace invalidation {

class Invalidation;
class ObjectId;

}  // namespace invalidation

namespace csync {

void RunAndDeleteClosure(invalidation::Closure* task);

bool RealModelTypeToObjectId(syncable::ModelType model_type,
                             invalidation::ObjectId* object_id);

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             syncable::ModelType* model_type);

std::string ObjectIdToString(const invalidation::ObjectId& object_id);

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation);

}  // namespace csync

#endif  // SYNC_NOTIFIER_INVALIDATION_UTIL_H_
