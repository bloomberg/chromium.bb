// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_controller_client_test_helper.h"

#include <set>

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "services/service_manager/public/cpp/connector.h"

class ChromeKeyboardControllerClientTestHelper::FakeKeyboardController
    : public ash::mojom::KeyboardController {
 public:
  FakeKeyboardController() = default;
  ~FakeKeyboardController() override = default;

  void BindRequest(ash::mojom::KeyboardControllerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // ash::mojom::KeyboardController:
  void GetKeyboardConfig(GetKeyboardConfigCallback callback) override {
    std::move(callback).Run(
        keyboard::mojom::KeyboardConfig::New(keyboard_config_));
  }
  void SetKeyboardConfig(
      keyboard::mojom::KeyboardConfigPtr keyboard_config) override {
    keyboard_config_ = *keyboard_config;
  }
  void IsKeyboardEnabled(IsKeyboardEnabledCallback callback) override {
    std::move(callback).Run(enabled_);
  }
  void SetEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override {
    keyboard_enable_flags_.insert(flag);
  }
  void ClearEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override {
    keyboard_enable_flags_.erase(flag);
  }
  void GetEnableFlags(GetEnableFlagsCallback callback) override {
    std::move(callback).Run(std::vector<keyboard::mojom::KeyboardEnableFlag>());
  }
  void ReloadKeyboardIfNeeded() override {}
  void RebuildKeyboardIfEnabled() override {}
  void IsKeyboardVisible(IsKeyboardVisibleCallback callback) override {
    std::move(callback).Run(visible_);
  }
  void ShowKeyboard() override { visible_ = true; }
  void HideKeyboard(ash::mojom::HideReason reason) override {
    visible_ = false;
  }
  void SetContainerType(keyboard::mojom::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        SetContainerTypeCallback callback) override {
    std::move(callback).Run(true);
  }
  void SetKeyboardLocked(bool locked) override {}
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override {}
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override {}
  void SetDraggableArea(const gfx::Rect& bounds) override {}
  void AddObserver(ash::mojom::KeyboardControllerObserverAssociatedPtrInfo
                       observer) override {}

 private:
  mojo::BindingSet<ash::mojom::KeyboardController> bindings_;
  keyboard::mojom::KeyboardConfig keyboard_config_;
  std::set<keyboard::mojom::KeyboardEnableFlag> keyboard_enable_flags_;
  bool enabled_ = false;
  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeKeyboardController);
};

// static
std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
ChromeKeyboardControllerClientTestHelper::InitializeForAsh() {
  auto helper = std::make_unique<ChromeKeyboardControllerClientTestHelper>();
  helper->Initialize(
      base::BindRepeating(&ChromeKeyboardControllerClientTestHelper::
                              AddKeyboardControllerBindingForAsh,
                          base::Unretained(helper.get())));
  return helper;
}

// static
std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
ChromeKeyboardControllerClientTestHelper::InitializeWithFake() {
  auto helper = std::make_unique<ChromeKeyboardControllerClientTestHelper>();
  helper->Initialize(
      base::BindRepeating(&ChromeKeyboardControllerClientTestHelper::
                              AddKeyboardControllerBindingForFake,
                          base::Unretained(helper.get())));
  return helper;
}

void ChromeKeyboardControllerClientTestHelper::Initialize(
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>
        bind_callback) {
  // Create a local service manager connector to handle requests to
  // ash::mojom::KeyboardController and bind to AshKeyboardController.
  service_manager::mojom::ConnectorRequest request;
  connector_ = service_manager::Connector::Create(&request);
  connector_->OverrideBinderForTesting(
      service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
      ash::mojom::KeyboardController::Name_, bind_callback);

  // Provide the local connector to ChromeKeyboardControllerClient.
  chrome_keyboard_controller_client_ =
      std::make_unique<ChromeKeyboardControllerClient>(connector_.get());
}

ChromeKeyboardControllerClientTestHelper::
    ChromeKeyboardControllerClientTestHelper() = default;

ChromeKeyboardControllerClientTestHelper::
    ~ChromeKeyboardControllerClientTestHelper() {
  chrome_keyboard_controller_client_.reset();
  connector_.reset();
}

void ChromeKeyboardControllerClientTestHelper::SetProfile(Profile* profile) {
  chrome_keyboard_controller_client_->set_profile_for_test(profile);
}

void ChromeKeyboardControllerClientTestHelper::
    AddKeyboardControllerBindingForAsh(mojo::ScopedMessagePipeHandle handle) {
  ash::Shell::Get()->ash_keyboard_controller()->BindRequest(
      ash::mojom::KeyboardControllerRequest(std::move(handle)));
}

void ChromeKeyboardControllerClientTestHelper::
    AddKeyboardControllerBindingForFake(mojo::ScopedMessagePipeHandle handle) {
  fake_controller_ = std::make_unique<FakeKeyboardController>();
  fake_controller_->BindRequest(
      ash::mojom::KeyboardControllerRequest(std::move(handle)));
}
