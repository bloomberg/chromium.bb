// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_DISPLAY_H_
#define ASH_HUD_DISPLAY_HUD_DISPLAY_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/graph.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace hud_display {

class DataSource;

// HUDDisplayView class can be used to display a system monitoring overview.
class HUDDisplayView : public views::WidgetDelegateView {
 public:
  // Default HUDDisplayView height.
  static constexpr size_t kDefaultHUDHeight = 300;

  // Border width inside the HUDDisplayView rectangle around contents.
  static constexpr size_t kHUDInset = 5;

  HUDDisplayView();
  ~HUDDisplayView() override;

  HUDDisplayView(const HUDDisplayView&) = delete;
  HUDDisplayView& operator=(const HUDDisplayView&) = delete;

  // view::
  void OnPaint(gfx::Canvas* canvas) override;

  // WidgetDelegate:
  views::ClientView* CreateClientView(views::Widget* widget) override;
  void OnWidgetInitialized() override;

  // Destroys global instance.
  static void Destroy();

  // Creates/Destroys global singleton.
  static void Toggle();

 private:
  // HUD is updatd with new data every tick.
  base::RepeatingTimer refresh_timer_;

  // --- Stacked:
  // Share of the total RAM occupied by Chrome browser private RSS.
  Graph graph_chrome_rss_private_;
  // Share of the total RAM reported as Free memory be kernel.
  Graph graph_mem_free_;
  // Total RAM - other graphs in this stack.
  Graph graph_mem_used_unknown_;
  // Share of the total RAM occupied by Chrome type=renderer processes private
  // RSS.
  Graph graph_renderers_rss_private_;
  // Share of the total RAM occupied by ARC++ processes private RSS.
  Graph graph_arc_rss_private_;
  // Share of the total RAM occupied by Chrome type=gpu process private RSS.
  Graph graph_gpu_rss_private_;
  // Share of the total RAM used by kernel GPU driver.
  Graph graph_gpu_kernel_;

  // Not stacked:
  // Share of the total RAM occupied by Chrome browser process shared RSS.
  Graph graph_chrome_rss_shared_;

  DataSource data_source_;

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_DISPLAY_H_
