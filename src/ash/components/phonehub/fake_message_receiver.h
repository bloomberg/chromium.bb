// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_

#include "ash/components/phonehub/message_receiver.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

class FakeMessageReceiver : public MessageReceiver {
 public:
  FakeMessageReceiver() = default;
  ~FakeMessageReceiver() override = default;

  using MessageReceiver::NotifyFetchCameraRollItemDataResponseReceived;
  using MessageReceiver::NotifyFetchCameraRollItemsResponseReceived;
  using MessageReceiver::NotifyPhoneStatusSnapshotReceived;
  using MessageReceiver::NotifyPhoneStatusUpdateReceived;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_MESSAGE_RECEIVER_H_
