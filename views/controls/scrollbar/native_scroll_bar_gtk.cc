// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/scrollbar/native_scroll_bar_gtk.h"

#include <gtk/gtk.h>

#include "ui/base/keycodes/keyboard_codes_posix.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/controls/scrollbar/scroll_bar.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarGtk, public:

NativeScrollBarGtk::NativeScrollBarGtk(NativeScrollBar* scroll_bar)
    : NativeControlGtk(),
      native_scroll_bar_(scroll_bar) {
  set_focus_view(scroll_bar);
}

NativeScrollBarGtk::~NativeScrollBarGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarGtk, View overrides:

void NativeScrollBarGtk::Layout() {
  SetBoundsRect(native_scroll_bar_->GetLocalBounds());
  NativeControlGtk::Layout();
}

gfx::Size NativeScrollBarGtk::GetPreferredSize() {
  if (native_scroll_bar_->IsHorizontal())
    return gfx::Size(0, GetHorizontalScrollBarHeight());
  return gfx::Size(GetVerticalScrollBarWidth(), 0);
}

// TODO(oshima|jcampan): key/mouse events are not delievered and
// the following code is not tested. It requires the focus manager to be fully
// implemented.
bool NativeScrollBarGtk::OnKeyPressed(const KeyEvent& event) {
  if (!native_view())
    return false;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      if (!native_scroll_bar_->IsHorizontal())
        MoveStep(false /* negative */);
      break;
    case ui::VKEY_DOWN:
      if (!native_scroll_bar_->IsHorizontal())
        MoveStep(true /* positive */);
      break;
    case ui::VKEY_LEFT:
      if (native_scroll_bar_->IsHorizontal())
        MoveStep(false /* negative */);
      break;
    case ui::VKEY_RIGHT:
      if (native_scroll_bar_->IsHorizontal())
        MoveStep(true /* positive */);
      break;
    case ui::VKEY_PRIOR:
      MovePage(false /* negative */);
      break;
    case ui::VKEY_NEXT:
      MovePage(true /* positive */);
      break;
    case ui::VKEY_HOME:
      MoveTo(0);
      break;
    case ui::VKEY_END:
      MoveToBottom();
      break;
    default:
      return false;
  }
  return true;
}

bool NativeScrollBarGtk::OnMouseWheel(const MouseWheelEvent& e) {
  if (!native_view())
    return false;
  MoveBy(-e.offset());  // e.GetOffset() > 0 means scroll up.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarGtk, NativeControlGtk overrides:

void NativeScrollBarGtk::CreateNativeControl() {
  GtkObject* adj = gtk_adjustment_new(native_scroll_bar_->GetMinPosition(),
                                      native_scroll_bar_->GetMinPosition(),
                                      native_scroll_bar_->GetMaxPosition(),
                                      10, 10,
                                      0);
  GtkWidget* widget;
  if (native_scroll_bar_->IsHorizontal()) {
    widget = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));
  } else {
    widget = gtk_vscrollbar_new(GTK_ADJUSTMENT(adj));
  }

  gtk_range_set_update_policy(GTK_RANGE(widget), GTK_UPDATE_CONTINUOUS);

  g_signal_connect(adj, "value-changed",
                   G_CALLBACK(CallValueChanged), this);

  NativeControlCreated(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarGtk, NativeScrollBarWrapper overrides:

int NativeScrollBarGtk::GetPosition() const {
  return static_cast<int>(gtk_range_get_value(GTK_RANGE(native_view())));
}

View* NativeScrollBarGtk::GetView() {
  return this;
}

void NativeScrollBarGtk::Update(int viewport_size,
                                int content_size,
                                int current_pos) {
  if (!native_view())
    return;

  if (content_size < 0)
    content_size = 0;

  if (current_pos < 0)
    current_pos = 0;

  if (current_pos > content_size)
    current_pos = content_size;

  ScrollBarController* controller = native_scroll_bar_->GetController();
  int step = controller->GetScrollIncrement(native_scroll_bar_,
                                            false /* step */,
                                            true /* positive */);
  int page = controller->GetScrollIncrement(native_scroll_bar_,
                                            true /* page */, true);
  GtkObject* adj = gtk_adjustment_new(current_pos,
                                      native_scroll_bar_->GetMinPosition(),
                                      content_size,
                                      step, page,
                                      viewport_size);
  gtk_range_set_adjustment(GTK_RANGE(native_view()), GTK_ADJUSTMENT(adj));
  g_signal_connect(adj, "value-changed", G_CALLBACK(CallValueChanged), this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarGtk, private:

void NativeScrollBarGtk::ValueChanged() {
  ScrollBarController* controller = native_scroll_bar_->GetController();
  controller->ScrollToPosition(native_scroll_bar_, GetPosition());
}

// static
void NativeScrollBarGtk::CallValueChanged(GtkWidget* widget,
                                          NativeScrollBarGtk* scroll_bar) {
  scroll_bar->ValueChanged();
}

void NativeScrollBarGtk::MoveBy(int o) {
  MoveTo(GetPosition() + o);
}

void NativeScrollBarGtk::MovePage(bool positive) {
  ScrollBarController* controller = native_scroll_bar_->GetController();
  MoveBy(controller->GetScrollIncrement(native_scroll_bar_, true, positive));
}

void NativeScrollBarGtk::MoveStep(bool positive) {
  ScrollBarController* controller = native_scroll_bar_->GetController();
  MoveBy(controller->GetScrollIncrement(native_scroll_bar_, false, positive));
}

void NativeScrollBarGtk::MoveTo(int p) {
  if (p < native_scroll_bar_->GetMinPosition())
    p = native_scroll_bar_->GetMinPosition();
  if (p > native_scroll_bar_->GetMaxPosition())
    p = native_scroll_bar_->GetMaxPosition();
  GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(native_view()));
  gtk_adjustment_set_value(adj, p);
}

void NativeScrollBarGtk::MoveToBottom() {
  GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(native_view()));
  MoveTo(adj->upper);
}

////////////////////////////////////////////////////////////////////////////////
// NativewScrollBarWrapper, public:

// static
NativeScrollBarWrapper* NativeScrollBarWrapper::CreateWrapper(
    NativeScrollBar* scroll_bar) {
  return new NativeScrollBarGtk(scroll_bar);
}

// static
int NativeScrollBarWrapper::GetHorizontalScrollBarHeight() {
  // TODO(oshima): get this from gtk's widget property "slider-width".
  return 20;
}

// static
int NativeScrollBarWrapper::GetVerticalScrollBarWidth() {
  // TODO(oshima): get this from gtk's widget property "slider-width".
  return 20;
}

}  // namespace views
