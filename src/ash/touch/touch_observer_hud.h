// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/values.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Label;
class View;
class Widget;
}

namespace ash {
class TouchHudCanvas;
class TouchLog;

// A heads-up display to show touch traces on the screen and log touch events.
// Implemented as an event filter which handles system level gesture events.
// Objects of this class manage their own lifetime.
class ASH_EXPORT TouchObserverHUD
    : public ui::EventHandler,
      public views::WidgetObserver,
      public display::DisplayObserver,
      public display::DisplayConfigurator::Observer,
      public WindowTreeHostManager::Observer {
 public:
  enum Mode {
    FULLSCREEN,
    REDUCED_SCALE,
    INVISIBLE,
  };

  explicit TouchObserverHUD(aura::Window* initial_root);
  ~TouchObserverHUD() override;

  // Clears touch points and traces from the screen.
  void Clear();

  // Removes the HUD from the screen.
  void Remove();

  // Returns the log of touch events for all displays as a dictionary mapping id
  // of each display to its touch log.
  static std::unique_ptr<base::DictionaryValue> GetAllAsDictionary();

  // Changes the display mode (e.g. scale, visibility). Calling this repeatedly
  // cycles between a fixed number of display modes.
  void ChangeToNextMode();

  // Returns log of touch events as a list value. Each item in the list is a
  // trace of one touch point.
  std::unique_ptr<base::ListValue> GetLogAsList() const;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // display::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;

  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanging() override;
  void OnDisplayConfigurationChanged() override;

 private:
  friend class TouchObserverHUDTest;

  void SetMode(Mode mode);

  void UpdateTouchPointLabel(int index);

  const int64_t display_id_;
  aura::Window* root_window_;
  views::Widget* widget_;

  static constexpr int kMaxTouchPoints = 32;

  Mode mode_;

  std::unique_ptr<TouchLog> touch_log_;

  TouchHudCanvas* canvas_;
  views::View* label_container_;
  views::Label* touch_labels_[kMaxTouchPoints];

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
