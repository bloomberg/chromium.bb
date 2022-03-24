// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_

#include "ash/components/phonehub/do_not_disturb_controller.h"
#include "ash/components/phonehub/find_my_device_controller.h"

namespace ash {
namespace phonehub {

class MessageSender;
class UserActionRecorder;

// Responsible for sending and receiving updates in regards to the Find My
// Device feature which involves ringing the user's remote phone.
class FindMyDeviceControllerImpl : public FindMyDeviceController {
 public:
  FindMyDeviceControllerImpl(MessageSender* message_sender,
                             UserActionRecorder* user_action_recorder);
  ~FindMyDeviceControllerImpl() override;

 private:
  friend class FindMyDeviceControllerImplTest;

  // FindMyDeviceController:
  void SetPhoneRingingStatusInternal(Status status) override;
  void RequestNewPhoneRingingState(bool ringing) override;
  Status GetPhoneRingingStatus() override;

  Status phone_ringing_status_ = Status::kRingingOff;

  MessageSender* message_sender_;
  UserActionRecorder* user_action_recorder_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FIND_MY_DEVICE_CONTROLLER_IMPL_H_
