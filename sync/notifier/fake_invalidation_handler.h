// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_
#define SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/notifier/invalidation_handler.h"

namespace syncer {

class FakeInvalidationHandler : public InvalidationHandler {
 public:
  FakeInvalidationHandler();
  virtual ~FakeInvalidationHandler();

  InvalidatorState GetInvalidatorState() const;
  const ObjectIdStateMap& GetLastInvalidationIdStateMap() const;
  IncomingInvalidationSource GetLastInvalidationSource() const;
  int GetInvalidationCount() const;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const ObjectIdStateMap& id_state_map,
      IncomingInvalidationSource source) OVERRIDE;

 private:
  InvalidatorState state_;
  ObjectIdStateMap last_id_state_map_;
  IncomingInvalidationSource last_source_;
  int invalidation_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationHandler);
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_FAKE_SYNC_NOTIFIER_OBSERVER_H_
