// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_ONBOARDING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_ONBOARDING_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

class SupervisionOnboardingScreen;

// Interface for dependency injection between SupervisionOnboardingScreen
// and its WebUI representation.
class SupervisionOnboardingScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"supervision-onboarding"};

  virtual ~SupervisionOnboardingScreenView() {}

  virtual void Bind(SupervisionOnboardingScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  SupervisionOnboardingScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisionOnboardingScreenView);
};

class SupervisionOnboardingScreenHandler
    : public BaseScreenHandler,
      public SupervisionOnboardingScreenView {
 public:
  using TView = SupervisionOnboardingScreenView;

  explicit SupervisionOnboardingScreenHandler(
      JSCallsContainer* js_calls_container);
  ~SupervisionOnboardingScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // SupervisionOnboardingScreenView:
  void Bind(SupervisionOnboardingScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // Callback used to bing the mojo onboarding controller.
  void BindSupervisionOnboardingController(
      supervision::mojom::OnboardingControllerRequest request);

  SupervisionOnboardingScreen* screen_ = nullptr;

  std::unique_ptr<supervision::OnboardingControllerImpl>
      supervision_onboarding_controller_;

  DISALLOW_COPY_AND_ASSIGN(SupervisionOnboardingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SUPERVISION_ONBOARDING_SCREEN_HANDLER_H_
