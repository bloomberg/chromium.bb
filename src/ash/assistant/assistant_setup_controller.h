// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"

namespace ash {

class AssistantControllerImpl;

class AssistantSetupController : public AssistantControllerObserver,
                                 public AssistantViewDelegateObserver {
 public:
  explicit AssistantSetupController(
      AssistantControllerImpl* assistant_controller);
  ~AssistantSetupController() override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantViewDelegateObserver:
  void OnOptInButtonPressed() override;

  void StartOnboarding(bool relaunch, FlowType type = FlowType::kConsentFlow);

 private:
  void OnOptInFlowFinished(bool relaunch, bool completed);

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  base::WeakPtrFactory<AssistantSetupController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSetupController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
