// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_TEST_WINDOW_MANAGER_CLIENT_H_
#define UI_AURA_TEST_MUS_TEST_WINDOW_MANAGER_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/mus_types.h"

namespace aura {

enum class WindowManagerClientChangeType {
  ADD_ACTIVATION_PARENT,
};

// WindowManagerClient implementation for tests.
class TestWindowManagerClient : public ui::mojom::WindowManagerClient {
 public:
  TestWindowManagerClient();
  ~TestWindowManagerClient() override;

  size_t GetChangeCountForType(WindowManagerClientChangeType type);

 private:
  // ui::mojom::WindowManagerClient:
  void AddActivationParent(Id transport_window_id) override;
  void RemoveActivationParent(Id transport_window_id) override;
  void ActivateNextWindow() override;
  void SetExtendedHitArea(Id window_id, const gfx::Insets& hit_area) override;
  void AddAccelerators(std::vector<ui::mojom::WmAcceleratorPtr> accelerators,
                       const AddAcceleratorsCallback& callback) override;
  void RemoveAccelerator(uint32_t id) override;
  void SetDisplayRoot(const display::Display& display,
                      ui::mojom::WmViewportMetricsPtr viewport_metrics,
                      bool is_primary_display,
                      Id window_id,
                      const SetDisplayRootCallback& callback) override;
  void WmResponse(uint32_t change_id, bool response) override;
  void WmSetBoundsResponse(uint32_t change_id) override;
  void WmRequestClose(Id transport_window_id) override;
  void WmSetFrameDecorationValues(
      ui::mojom::FrameDecorationValuesPtr values) override;
  void WmSetNonClientCursor(uint32_t window_id,
                            ui::CursorData cursor_data) override;
  void WmLockCursor() override;
  void WmUnlockCursor() override;
  void WmSetCursorVisible(bool visible) override;
  void WmSetGlobalOverrideCursor(
      base::Optional<ui::CursorData> cursor) override;
  void OnWmCreatedTopLevelWindow(uint32_t change_id,
                                 Id transport_window_id) override;
  void OnAcceleratorAck(
      uint32_t event_id,
      ui::mojom::EventResult result,
      const std::unordered_map<std::string, std::vector<uint8_t>>& properties)
      override;

  std::vector<WindowManagerClientChangeType> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerClient);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_TEST_WINDOW_MANAGER_CLIENT_H_
