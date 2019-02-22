// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

class AssistantController;

// The class to manage assistant notifications.
class ASH_EXPORT AssistantNotificationController
    : public chromeos::assistant::mojom::AssistantNotificationSubscriber {
 public:
  using AssistantNotificationPtr =
      chromeos::assistant::mojom::AssistantNotificationPtr;

  explicit AssistantNotificationController(
      AssistantController* assistant_controller);
  ~AssistantNotificationController() override;

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(chromeos::assistant::mojom::Assistant* assistant);

  void RetrieveNotification(AssistantNotificationPtr notification,
                            int action_index);
  void DismissNotification(AssistantNotificationPtr notification);

  // chromeos::assistant::mojom::AssistantNotificationSubscriber:
  void OnShowNotification(AssistantNotificationPtr notification) override;
  void OnRemoveNotification(const std::string& grouping_id) override;

 private:
  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojo::Binding<chromeos::assistant::mojom::AssistantNotificationSubscriber>
      assistant_notification_subscriber_binding_;

  // Owned by AssistantController.
  chromeos::assistant::mojom::Assistant* assistant_ = nullptr;

  const message_center::NotifierId notifier_id_;

  base::WeakPtrFactory<AssistantNotificationController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_H_
