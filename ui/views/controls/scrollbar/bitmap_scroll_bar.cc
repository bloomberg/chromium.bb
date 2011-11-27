// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/bitmap_scroll_bar.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/widget/widget.h"
#include "views/controls/menu/menu.h"
#include "views/controls/scroll_view.h"

#if defined(OS_LINUX)
#include "views/screen.h"
#endif

#undef min
#undef max

namespace views {

namespace {

// The distance the mouse can be dragged outside the bounds of the thumb during
// dragging before the scrollbar will snap back to its regular position.
const int kScrollThumbDragOutSnap = 100;

///////////////////////////////////////////////////////////////////////////////
//
// AutorepeatButton
//
//  A button that activates on mouse pressed rather than released, and that
//  continues to fire the clicked action as the mouse button remains pressed
//  down on the button.
//
///////////////////////////////////////////////////////////////////////////////
class AutorepeatButton : public ImageButton {
 public:
  explicit AutorepeatButton(ButtonListener* listener)
      : ImageButton(listener),
        ALLOW_THIS_IN_INITIALIZER_LIST(repeater_(
            base::Bind(&AutorepeatButton::NotifyClick,
                       base::Unretained(this)))) {
  }
  virtual ~AutorepeatButton() {}

 protected:
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE {
    Button::NotifyClick(event);
    repeater_.Start();
    return true;
  }

  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE {
    OnMouseCaptureLost();
  }

  virtual void OnMouseCaptureLost() OVERRIDE {
    repeater_.Stop();
  }

 private:
  void NotifyClick() {
#if defined(OS_WIN)
    DWORD pos = GetMessagePos();
    POINTS points = MAKEPOINTS(pos);
    gfx::Point cursor_point(points.x, points.y);
#elif defined(OS_LINUX)
    gfx::Point cursor_point = Screen::GetCursorScreenPoint();
#endif
    views::MouseEvent event(ui::ET_MOUSE_RELEASED,
                            cursor_point.x(), cursor_point.y(),
                            ui::EF_LEFT_BUTTON_DOWN);
    Button::NotifyClick(event);
  }

  // The repeat controller that we use to repeatedly click the button when the
  // mouse button is down.
  RepeatController repeater_;

  DISALLOW_COPY_AND_ASSIGN(AutorepeatButton);
};

///////////////////////////////////////////////////////////////////////////////
//
// BitmapScrollBarThumb
//
//  A view that acts as the thumb in the scroll bar track that the user can
//  drag to scroll the associated contents view within the viewport.
//
///////////////////////////////////////////////////////////////////////////////
class BitmapScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit BitmapScrollBarThumb(BitmapScrollBar* scroll_bar)
      : BaseScrollBarThumb(scroll_bar),
        scroll_bar_(scroll_bar) {
  }
  virtual ~BitmapScrollBarThumb() { }

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(background_bitmap()->width(),
                     start_cap_bitmap()->height() +
                         end_cap_bitmap()->height() +
                         grippy_bitmap()->height());
  }

 protected:
  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawBitmapInt(*start_cap_bitmap(), 0, 0);
    int top_cap_height = start_cap_bitmap()->height();
    int bottom_cap_height = end_cap_bitmap()->height();
    int thumb_body_height = height() - top_cap_height - bottom_cap_height;
    canvas->TileImageInt(*background_bitmap(), 0, top_cap_height,
                         background_bitmap()->width(), thumb_body_height);
    canvas->DrawBitmapInt(*end_cap_bitmap(), 0,
                          height() - bottom_cap_height);

    // Paint the grippy over the track.
    int grippy_x = (width() - grippy_bitmap()->width()) / 2;
    int grippy_y = (thumb_body_height - grippy_bitmap()->height()) / 2;
    canvas->DrawBitmapInt(*grippy_bitmap(), grippy_x, grippy_y);
  }

 private:
  // Returns the bitmap rendered at the start of the thumb.
  SkBitmap* start_cap_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_START_CAP][GetState()];
  }

  // Returns the bitmap rendered at the end of the thumb.
  SkBitmap* end_cap_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_END_CAP][GetState()];
  }

  // Returns the bitmap that is tiled in the background of the thumb between
  // the start and the end caps.
  SkBitmap* background_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_MIDDLE][GetState()];
  }

  // Returns the bitmap that is rendered in the middle of the thumb
  // transparently over the background bitmap.
  SkBitmap* grippy_bitmap() const {
    return scroll_bar_->images_[BitmapScrollBar::THUMB_GRIPPY]
        [CustomButton::BS_NORMAL];
  }

  // The BitmapScrollBar that owns us.
  BitmapScrollBar* scroll_bar_;

  DISALLOW_COPY_AND_ASSIGN(BitmapScrollBarThumb);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, public:

BitmapScrollBar::BitmapScrollBar(bool horizontal, bool show_scroll_buttons)
    : BaseScrollBar(horizontal, new BitmapScrollBarThumb(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(prev_button_(new AutorepeatButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(next_button_(new AutorepeatButton(this))),
      show_scroll_buttons_(show_scroll_buttons) {
  if (!show_scroll_buttons_) {
    prev_button_->SetVisible(false);
    next_button_->SetVisible(false);
  }

  AddChildView(prev_button_);
  AddChildView(next_button_);

  set_context_menu_controller(this);
  prev_button_->set_context_menu_controller(this);
  next_button_->set_context_menu_controller(this);
}

void BitmapScrollBar::SetImage(ScrollBarPart part,
                               CustomButton::ButtonState state,
                               SkBitmap* bitmap) {
  DCHECK(part < PART_COUNT);
  DCHECK(state < CustomButton::BS_COUNT);
  switch (part) {
    case PREV_BUTTON:
      prev_button_->SetImage(state, bitmap);
      break;
    case NEXT_BUTTON:
      next_button_->SetImage(state, bitmap);
      break;
    case THUMB_START_CAP:
    case THUMB_MIDDLE:
    case THUMB_END_CAP:
    case THUMB_GRIPPY:
    case THUMB_TRACK:
      images_[part][state] = bitmap;
      break;
  }
}

int BitmapScrollBar::GetLayoutSize() const {
  gfx::Size prefsize = prev_button_->GetPreferredSize();
  return IsHorizontal() ? prefsize.height() : prefsize.width();
}

gfx::Rect BitmapScrollBar::GetTrackBounds() const {
  gfx::Size prefsize = prev_button_->GetPreferredSize();
  if (IsHorizontal()) {
    if (!show_scroll_buttons_)
      prefsize.set_width(0);
    int new_width =
        std::max(0, width() - (prefsize.width() * 2));
    gfx::Rect track_bounds(prefsize.width(), 0, new_width, prefsize.height());
    return track_bounds;
  }
  if (!show_scroll_buttons_)
    prefsize.set_height(0);
  gfx::Rect track_bounds(0, prefsize.height(), prefsize.width(),
                         std::max(0, height() - (prefsize.height() * 2)));
  return track_bounds;
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, View implementation:

gfx::Size BitmapScrollBar::GetPreferredSize() {
  // In this case, we're returning the desired width of the scrollbar and its
  // minimum allowable height.
  gfx::Size button_prefsize = prev_button_->GetPreferredSize();
  return gfx::Size(button_prefsize.width(), button_prefsize.height() * 2);
}

void BitmapScrollBar::Layout() {
  // Size and place the two scroll buttons.
  if (show_scroll_buttons_) {
    gfx::Size prefsize = prev_button_->GetPreferredSize();
    prev_button_->SetBounds(0, 0, prefsize.width(), prefsize.height());
    prefsize = next_button_->GetPreferredSize();
    if (IsHorizontal()) {
      next_button_->SetBounds(width() - prefsize.width(), 0, prefsize.width(),
                              prefsize.height());
    } else {
      next_button_->SetBounds(0, height() - prefsize.height(), prefsize.width(),
                              prefsize.height());
    }
  } else {
    prev_button_->SetBounds(0, 0, 0, 0);
    next_button_->SetBounds(0, 0, 0, 0);
  }

  BaseScrollBarThumb* thumb = GetThumb();
  // Size and place the thumb
  gfx::Size thumb_prefsize = thumb->GetPreferredSize();
  gfx::Rect track_bounds = GetTrackBounds();

  // Preserve the height/width of the thumb (depending on orientation) as set
  // by the last call to |Update|, but coerce the width/height to be the
  // appropriate value for the bitmaps provided.
  if (IsHorizontal()) {
    thumb->SetBounds(thumb->x(), thumb->y(), thumb->width(),
                      thumb_prefsize.height());
  } else {
    thumb->SetBounds(thumb->x(), thumb->y(), thumb_prefsize.width(),
                     thumb->height());
  }

  // Hide the thumb if the track isn't tall enough to display even a tiny
  // thumb. The user can only use the mousewheel, scroll buttons or keyboard
  // in this scenario.
  if ((IsHorizontal() && (track_bounds.width() < thumb_prefsize.width()) ||
      (!IsHorizontal() && (track_bounds.height() < thumb_prefsize.height())))) {
    thumb->SetVisible(false);
  } else if (!thumb->IsVisible()) {
    thumb->SetVisible(true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, View implementation:

void BitmapScrollBar::OnPaint(gfx::Canvas* canvas) {
  // Paint the track.
  gfx::Rect track_bounds = GetTrackBounds();
  canvas->TileImageInt(*images_[THUMB_TRACK][GetThumbTrackState()],
                       track_bounds.x(), track_bounds.y(),
                       track_bounds.width(), track_bounds.height());
}

///////////////////////////////////////////////////////////////////////////////
// BitmapScrollBar, ButtonListener implementation:

void BitmapScrollBar::ButtonPressed(Button* sender, const views::Event& event) {
  if (sender == prev_button_) {
    ScrollByAmount(SCROLL_PREV_LINE);
  } else if (sender == next_button_) {
    ScrollByAmount(SCROLL_NEXT_LINE);
  }
}

}  // namespace views
