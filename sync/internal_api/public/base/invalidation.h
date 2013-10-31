// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/ack_handle.h"
#include "sync/internal_api/public/util/weak_handle.h"

namespace syncer {

class DroppedInvalidationTracker;
class AckHandler;

// Represents a local invalidation, and is roughly analogous to
// invalidation::Invalidation.  Unlike invalidation::Invalidation, this class
// supports "local" ack-tracking and simple serialization to pref values.
class SYNC_EXPORT Invalidation {
 public:
  // Factory functions.
  static Invalidation Init(
      const invalidation::ObjectId& id,
      int64 version,
      const std::string& payload);
  static Invalidation InitUnknownVersion(const invalidation::ObjectId& id);
  static Invalidation InitFromDroppedInvalidation(const Invalidation& dropped);
  static scoped_ptr<Invalidation> InitFromValue(
      const base::DictionaryValue& value);

  ~Invalidation();

  // Compares two invalidations.  The comparison ignores ack-tracking state.
  bool Equals(const Invalidation& other) const;

  invalidation::ObjectId object_id() const;
  bool is_unknown_version() const;

  // Safe to call only if is_unknown_version() returns false.
  int64 version() const;

  // Safe to call only if is_unknown_version() returns false.
  const std::string& payload() const;

  const AckHandle& ack_handle() const;

  // TODO(rlarocque): Remove this method and use AckHandlers instead.
  void set_ack_handle(const AckHandle& ack_handle);

  // Functions from the alternative ack tracking framework.
  // Currently unused.
  void set_ack_handler(syncer::WeakHandle<AckHandler> ack_handler);
  bool SupportsAcknowledgement() const;
  void Acknowledge() const;

  // Drops an invalidation.
  //
  // The drop record will be tracked by the specified
  // DroppedInvalidationTracker.  The caller should hang on to this tracker.  It
  // will need to use it when it recovers from this drop event.  See the
  // documentation of DroppedInvalidationTracker for more details.
  void Drop(DroppedInvalidationTracker* tracker) const;

  scoped_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

 private:
  Invalidation(const invalidation::ObjectId& id,
               bool is_unknown_version,
               int64 version,
               const std::string& payload,
               AckHandle ack_handle);

  // The ObjectId to which this invalidation belongs.
  invalidation::ObjectId id_;

  // This flag is set to true if this is an unknown version invalidation.
  bool is_unknown_version_;

  // The version number of this invalidation.  Should not be accessed if this is
  // an unkown version invalidation.
  int64 version_;

  // The payaload associated with this invalidation.  Should not be accessed if
  // this is an unknown version invalidation.
  std::string payload_;

  // A locally generated unique ID used to manage local acknowledgements.
  AckHandle ack_handle_;
  syncer::WeakHandle<AckHandler> ack_handler_;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_H_
