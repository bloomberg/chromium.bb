// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_DESKTOP_POPUP_ALIGNMENT_DELEGATE_H_
#define UI_MESSAGE_CENTER_VIEWS_DESKTOP_POPUP_ALIGNMENT_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/popup_alignment_delegate.h"

namespace gfx {
class Screen;
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
}

// The PopupAlignmentDelegate for non-ash Windows/Linux desktop.
class MESSAGE_CENTER_EXPORT DesktopPopupAlignmentDelegate
    : public PopupAlignmentDelegate,
      public gfx::DisplayObserver {
 public:
  DesktopPopupAlignmentDelegate();
  ~DesktopPopupAlignmentDelegate() override;

  void StartObserving(gfx::Screen* screen);

  // Overridden from PopupAlignmentDelegate:
  int GetToastOriginX(const gfx::Rect& toast_bounds) const override;
  int GetBaseLine() const override;
  int GetWorkAreaBottom() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  void RecomputeAlignment(const gfx::Display& display) override;

 private:
  friend class test::MessagePopupCollectionTest;

  enum PopupAlignment {
    POPUP_ALIGNMENT_TOP = 1 << 0,
    POPUP_ALIGNMENT_LEFT = 1 << 1,
    POPUP_ALIGNMENT_BOTTOM = 1 << 2,
    POPUP_ALIGNMENT_RIGHT = 1 << 3,
  };

  // Overridden from gfx::DisplayObserver:
  void OnDisplayAdded(const gfx::Display& new_display) override;
  void OnDisplayRemoved(const gfx::Display& old_display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override;

  int32_t alignment_;
  int64_t display_id_;
  gfx::Screen* screen_;
  gfx::Rect work_area_;

  DISALLOW_COPY_AND_ASSIGN(DesktopPopupAlignmentDelegate);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_DESKTOP_POPUP_ALIGNMENT_DELEGATE_H_
