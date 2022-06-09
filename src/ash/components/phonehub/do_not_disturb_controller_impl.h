// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_

#include "ash/components/phonehub/do_not_disturb_controller.h"

namespace ash {
namespace phonehub {

class MessageSender;
class UserActionRecorder;

// Responsible for sending and receiving states in regards to the DoNotDisturb
// feature of the user's remote phone.
class DoNotDisturbControllerImpl : public DoNotDisturbController {
 public:
  DoNotDisturbControllerImpl(MessageSender* message_sender,
                             UserActionRecorder* user_action_recorder);
  ~DoNotDisturbControllerImpl() override;

 private:
  friend class DoNotDisturbControllerImplTest;

  // DoNotDisturbController:
  bool IsDndEnabled() const override;
  void SetDoNotDisturbStateInternal(bool is_dnd_enabled,
                                    bool can_request_new_dnd_state) override;
  void RequestNewDoNotDisturbState(bool enabled) override;
  bool CanRequestNewDndState() const override;

  MessageSender* message_sender_;
  UserActionRecorder* user_action_recorder_;

  bool is_dnd_enabled_ = false;
  bool can_request_new_dnd_state_ = false;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_DO_NOT_DISTURB_CONTROLLER_IMPL_H_
