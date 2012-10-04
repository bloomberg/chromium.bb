// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace

namespace syncer {

// Opaque class that represents an ack handle.
// TODO(dcheng): This is just a refactoring change, so the class is empty for
// the moment. It will be filled once we start implementing 'reminders'.
class AckHandle {
 public:
  bool Equals(const AckHandle& other) const;

  scoped_ptr<base::Value> ToValue() const;

  bool ResetFromValue(const base::Value& value);
};

// Represents a local invalidation, and is roughly analogous to
// invalidation::Invalidation. It contains a payload (which may be empty) and an
// associated ack handle that an InvalidationHandler implementation can use to
// acknowledge receipt of the invalidation. It does not embed the object ID,
// since it is typically associated with it through ObjectIdInvalidationMap.
struct Invalidation {
  std::string payload;
  AckHandle ack_handle;

  bool Equals(const Invalidation& other) const;

  // Caller owns the returned DictionaryValue.
  scoped_ptr<base::DictionaryValue> ToValue() const;

  bool ResetFromValue(const base::DictionaryValue& value);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_
