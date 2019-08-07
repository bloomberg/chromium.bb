// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_

#include <vector>

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Fake implementation of ash's mojo AccessibilityController interface.
//
// This fake registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class FakeAccessibilityController : ash::mojom::AccessibilityController {
 public:
  FakeAccessibilityController();
  ~FakeAccessibilityController() override;

  bool was_client_set() const { return was_client_set_; }

  // ash::mojom::AccessibilityController:
  void SetClient(ash::mojom::AccessibilityControllerClientPtr client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override;
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override;
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override;
  void SetAccessibilityPanelBounds(
      const gfx::Rect& bounds,
      ash::mojom::AccessibilityPanelState state) override;
  void SetSelectToSpeakState(ash::mojom::SelectToSpeakState state) override;
  void SetSelectToSpeakEventHandlerDelegate(
      ash::mojom::SelectToSpeakEventHandlerDelegatePtr delegate) override;
  void SetSwitchAccessEventHandlerDelegate(
      ash::mojom::SwitchAccessEventHandlerDelegatePtr delegate) override;
  void SetSwitchAccessKeysToCapture(
      const std::vector<int>& keys_to_capture) override;
  void ToggleDictationFromSource(
      ash::mojom::DictationToggleSource source) override;
  void ForwardKeyEventsToSwitchAccess(bool should_forward) override;
  void GetBatteryDescription(GetBatteryDescriptionCallback callback) override;
  void SetVirtualKeyboardVisible(bool is_visible) override;

 private:
  void Bind(mojo::ScopedMessagePipeHandle handle);

  mojo::Binding<ash::mojom::AccessibilityController> binding_{this};
  bool was_client_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityController);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
