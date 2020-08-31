// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_
#define COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace syncer {

class SingleObjectInvalidationSet;
class TopicInvalidationMap;
class AckHandle;
class UnackedInvalidationSet;

using UnackedInvalidationsMap = std::map<Topic, UnackedInvalidationSet>;

// Manages the set of invalidations that are awaiting local acknowledgement for
// a particular Topic.  This set of invalidations will be persisted across
// restarts, though this class is not directly responsible for that.
class INVALIDATION_EXPORT UnackedInvalidationSet {
 public:
  static const size_t kMaxBufferedInvalidations;

  explicit UnackedInvalidationSet(const Topic& topic);
  UnackedInvalidationSet(const UnackedInvalidationSet& other);
  ~UnackedInvalidationSet();

  // Returns the Topic of the invalidations this class is tracking.
  const Topic& topic() const;

  // Adds a new invalidation to the set awaiting acknowledgement.
  void Add(const Invalidation& invalidation);

  // Adds many new invalidations to the set awaiting acknowledgement.
  void AddSet(const SingleObjectInvalidationSet& invalidations);

  // Exports the set of invalidations awaiting acknowledgement as an
  // TopicInvalidationMap. Each of these invalidations will be associated
  // with the given |ack_handler|.
  //
  // The contents of the UnackedInvalidationSet are not directly modified by
  // this procedure, but the AckHandles stored in those exported invalidations
  // are likely to end up back here in calls to Acknowledge() or Drop().
  void ExportInvalidations(
      base::WeakPtr<AckHandler> ack_handler,
      scoped_refptr<base::SingleThreadTaskRunner> ack_handler_task_runner,
      TopicInvalidationMap* out) const;

  // Removes all stored invalidations from this object.
  void Clear();

  // Indicates that a handler has registered to handle these invalidations.
  //
  // Registrations with the invalidations server persist across restarts, but
  // registrations from InvalidationHandlers to the InvalidationService are not.
  // In the time immediately after a restart, it's possible that the server
  // will send us invalidations, and we won't have a handler to send them to.
  //
  // The SetIsRegistered() call indicates that this period has come to an end.
  // There is now a handler that can receive these invalidations.  Once this
  // function has been called, the kMaxBufferedInvalidations limit will be
  // ignored.  It is assumed that the handler will manage its own buffer size.
  void SetHandlerIsRegistered();

  // Indicates that the handler has now unregistered itself.
  //
  // This causes the object to resume enforcement of the
  // kMaxBufferedInvalidations limit.
  void SetHandlerIsUnregistered();

  // Given an AckHandle belonging to one of the contained invalidations, finds
  // the invalidation and drops it from the list.  It is considered to be
  // acknowledged, so there is no need to continue maintaining its state.
  void Acknowledge(const AckHandle& handle);

  // Given an AckHandle belonging to one of the contained invalidations, finds
  // the invalidation, drops it from the list, and adds additional state to
  // indicate that this invalidation has been lost without being acted on.
  void Drop(const AckHandle& handle);

 private:

  typedef std::set<Invalidation, InvalidationVersionLessThan> InvalidationsSet;

  // Limits the list size to the given maximum.  This function will correctly
  // update this class' internal data to indicate if invalidations have been
  // dropped.
  void Truncate(size_t max_size);

  bool registered_;
  const Topic topic_;
  InvalidationsSet invalidations_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_
