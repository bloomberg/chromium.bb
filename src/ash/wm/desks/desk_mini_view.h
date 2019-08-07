// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {

class CloseDeskButton;
class DesksBarView;
class DeskPreviewView;

// A view that acts as a mini representation (a.k.a. desk thumbnail) of a
// virtual desk in the desk bar view when overview mode is active. This view
// shows a preview of the contents of the associated desk, its title, and
// supports desk activation and removal.
class ASH_EXPORT DeskMiniView : public views::Button,
                                public views::ButtonListener,
                                public Desk::Observer {
 public:
  DeskMiniView(DesksBarView* owner_bar,
               aura::Window* root_window,
               Desk* desk,
               const base::string16& title);
  ~DeskMiniView() override;

  aura::Window* root_window() { return root_window_; }

  Desk* desk() { return desk_; }

  const CloseDeskButton* close_desk_button() const {
    return close_desk_button_;
  }

  void SetTitle(const base::string16& title);

  // Returns the associated desk's container window on the display this
  // mini_view resides on.
  aura::Window* GetDeskContainer() const;

  // Updates the visibility state of the close button depending on whether this
  // view is mouse hovered.
  void OnHoverStateMayHaveChanged();

  // Updates the border color of the DeskPreviewView based on the activation
  // state of the corresponding desk.
  void UpdateBorderColor();

  // views::Button:
  const char* GetClassName() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Desk::Observer:
  void OnContentChanged() override;
  void OnDeskDestroyed(const Desk* desk) override;

  bool IsPointOnMiniView(const gfx::Point& screen_location) const;

 private:
  DesksBarView* const owner_bar_;

  // The root window on which this mini_view is created.
  aura::Window* root_window_;

  // The associated desk. Can be null when the desk is deleted before this
  // mini_view completes its removal animation. See comment above
  // OnDeskRemoved().
  Desk* desk_;  // Not owned.

  // The view that shows a preview of the desk contents.
  std::unique_ptr<DeskPreviewView> desk_preview_;

  // The desk title.
  views::Label* label_;

  // The close button that shows on hover.
  CloseDeskButton* close_desk_button_;

  DISALLOW_COPY_AND_ASSIGN(DeskMiniView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_H_
