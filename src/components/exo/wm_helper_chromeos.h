// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_
#define COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_

#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace ash {
class TabletModeObserver;
}

namespace aura {
class env;
class Window;
namespace client {
class CursorClient;
class FocusChangeObserver;
}  // namespace client
}  // namespace aura

namespace wm {
class ActivationChangeObserver;
}

namespace display {
class ManagedDisplayInfo;
}

namespace ui {
class EventHandler;
class DropTargetEvent;
}  // namespace ui

namespace wm {
class ActivationChangeObserver;
}

namespace exo {

// A ChromeOS-specific helper class for accessing WindowManager related
// features.
class WMHelperChromeOS : public WMHelper {
 public:
  explicit WMHelperChromeOS(aura::Env* env);
  ~WMHelperChromeOS() override;
  static WMHelperChromeOS* GetInstance();
  void AddTabletModeObserver(ash::TabletModeObserver* observer);
  void RemoveTabletModeObserver(ash::TabletModeObserver* observer);
  void AddDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);
  void RemoveDisplayConfigurationObserver(
      ash::WindowTreeHostManager::Observer* observer);

  // Overridden from WMHelper
  aura::Env* env() override;
  void AddActivationObserver(wm::ActivationChangeObserver* observer) override;
  void RemoveActivationObserver(
      wm::ActivationChangeObserver* observer) override;
  void AddFocusObserver(aura::client::FocusChangeObserver* observer) override;
  void RemoveFocusObserver(
      aura::client::FocusChangeObserver* observer) override;
  void AddDragDropObserver(DragDropObserver* observer) override;
  void RemoveDragDropObserver(DragDropObserver* observer) override;
  void SetDragDropDelegate(aura::Window*) override;
  void ResetDragDropDelegate(aura::Window*) override;
  void AddVSyncObserver(
      ui::CompositorVSyncManager::Observer* observer) override;
  void RemoveVSyncObserver(
      ui::CompositorVSyncManager::Observer* observer) override;

  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override;
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const override;

  aura::Window* GetPrimaryDisplayContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  aura::Window* GetRootWindowForNewWindows() const override;
  aura::client::CursorClient* GetCursorClient() override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool IsTabletModeWindowManagerEnabled() const override;
  double GetDefaultDeviceScaleFactor() const override;

  LifetimeManager* GetLifetimeManager() override;
  aura::client::CaptureClient* GetCaptureClient() override;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

 private:
  base::ObserverList<DragDropObserver>::Unchecked drag_drop_observers_;
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  aura::Env* const env_;
  LifetimeManager lifetime_manager_;

  DISALLOW_COPY_AND_ASSIGN(WMHelperChromeOS);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_CHROMEOS_H_
