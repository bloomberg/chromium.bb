// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "base/macros.h"

namespace ash {

class AssistantController;

class AssistantSetupController : public AssistantControllerObserver,
                                 public AssistantOptInDelegate {
 public:
  explicit AssistantSetupController(AssistantController* assistant_controller);
  ~AssistantSetupController() override;

  // Sets the controller's internal |assistant_setup_| reference.
  void SetAssistantSetup(mojom::AssistantSetup* assistant_setup);

  // AssistantControllerObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantOptInDelegate:
  void OnOptInButtonPressed() override;

 private:
  void StartOnboarding(bool relaunch);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  mojom::AssistantSetup* assistant_setup_ =
      nullptr;  // Owned by AssistantController.

  DISALLOW_COPY_AND_ASSIGN(AssistantSetupController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
