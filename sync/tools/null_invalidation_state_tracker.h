// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_
#define SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/invalidation_state_tracker.h"

namespace syncer {

class NullInvalidationStateTracker
    : public base::SupportsWeakPtr<NullInvalidationStateTracker>,
      public InvalidationStateTracker {
 public:
  NullInvalidationStateTracker();
  virtual ~NullInvalidationStateTracker();

  virtual void ClearAndSetNewClientId(const std::string& data) override;
  virtual std::string GetInvalidatorClientId() const override;

  virtual std::string GetBootstrapData() const override;
  virtual void SetBootstrapData(const std::string& data) override;

  virtual void SetSavedInvalidations(
      const UnackedInvalidationsMap& states) override;
  virtual UnackedInvalidationsMap GetSavedInvalidations() const override;

  virtual void Clear() override;
};

}  // namespace syncer

#endif  // SYNC_TOOLS_NULL_INVALIDATION_STATE_TRACKER_H_
