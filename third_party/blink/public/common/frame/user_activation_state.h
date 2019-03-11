// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_STATE_H_

#include "base/time/time_override.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// This class represents the user activation state of a frame.  It maintains two
// bits of information: whether this frame has ever seen an activation in its
// lifetime, and whether this frame has a current activation that was neither
// expired nor consumed.
//
// This provides a simple alternative to current user gesture tracking code
// based on UserGestureIndicator and UserGestureToken.
class BLINK_COMMON_EXPORT UserActivationState {
 public:
  void Activate();
  void Clear();

  // Returns the sticky activation state, which is |true| if the frame has ever
  // seen an activation.
  bool HasBeenActive() const { return has_been_active_; }

  // Returns the transient activation state, which is |true| if the frame has
  // recently been activated and the transient state hasn't been consumed yet.
  bool IsActive() const;

  // Consumes the transient activation state if available, and returns |true| if
  // successfully consumed.
  bool ConsumeIfActive();

  // Transfers user activation state from |other| into |this|:
  // - The sticky bit in |this| gets set if the bit in |other| is set.
  // - The transient expiry time in |this| becomes the max of the expiry times
  //   in |this| and |other|.
  // - The state in |other| is cleared.
  void TransferFrom(UserActivationState& other);

 private:
  void ActivateTransientState();
  void DeactivateTransientState();

  bool has_been_active_ = false;
  base::TimeTicks transient_state_expiry_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_STATE_H_
