// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Implements a contextual nudge for multipaste.
class ASH_EXPORT ClipboardNudge : public ShelfObserver {
 public:
  ClipboardNudge();
  ClipboardNudge(const ClipboardNudge&) = delete;
  ClipboardNudge& operator=(const ClipboardNudge&) = delete;
  ~ClipboardNudge() override;

  // ShelfObserver overrides:
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;
  void Close();

  views::Widget* widget() { return widget_.get(); }

 private:
  class ClipboardNudgeView;

  // Calculate and set widget bounds based ona fixed width and a variable
  // height to correctly fit the text.
  void CalculateAndSetWidgetBounds();

  std::unique_ptr<views::Widget> widget_;

  ClipboardNudgeView* nudge_view_ = nullptr;  // not_owned

  aura::Window* const root_window_;

  ScopedObserver<Shelf, ShelfObserver> shelf_observer_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_H_