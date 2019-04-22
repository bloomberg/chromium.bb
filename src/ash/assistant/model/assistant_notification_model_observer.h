// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class AssistantNotification;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

// A checked observer which receives notification of changes to the Assistant
// notification model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantNotificationModelObserver
    : public base::CheckedObserver {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;

  // Invoked when the specified |notification| has been added.
  virtual void OnNotificationAdded(const AssistantNotification* notification) {}

  // Invoked when the specified |notification| has been updated.
  virtual void OnNotificationUpdated(
      const AssistantNotification* notification) {}

  // Invoked when the specified |notification| has been removed. If
  // |from_server| is true the request to remove was initiated by the server.
  virtual void OnNotificationRemoved(const AssistantNotification* notification,
                                     bool from_server) {}

  // Invoked when all notifications have been removed. If |from_server| is true
  // the request to remove was initiated by the server.
  virtual void OnAllNotificationsRemoved(bool from_server) {}

 protected:
  ~AssistantNotificationModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_NOTIFICATION_MODEL_OBSERVER_H_
