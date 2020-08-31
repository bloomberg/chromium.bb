// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// Interface for a class that holds Assistant related prefs and states.
class ASH_PUBLIC_EXPORT AssistantState : public AssistantStateBase {
 public:
  static AssistantState* Get();

  AssistantState();
  ~AssistantState() override;

  void NotifyStatusChanged(chromeos::assistant::AssistantStatus status);
  void NotifyFeatureAllowed(chromeos::assistant::AssistantAllowedState state);
  void NotifyLocaleChanged(const std::string& locale);
  void NotifyArcPlayStoreEnabledChanged(bool enabled);
  void NotifyLockedFullScreenStateChanged(bool enabled);

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantState);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_H_
