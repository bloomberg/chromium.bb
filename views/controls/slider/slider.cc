// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/slider/slider.h"

#include <string>

#include "views/controls/slider/native_slider_wrapper.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget.h"

namespace views {

// static
const char Slider::kViewClassName[] = "views/Slider";

/////////////////////////////////////////////////////////////////////////////
// Slider

Slider::Slider()
    : native_wrapper_(NULL),
      listener_(NULL),
      style_(STYLE_HORIZONTAL) {
  SetFocusable(true);
}

Slider::Slider(double min, double max, double step, StyleFlags style,
    SliderListener* listener)
    : native_wrapper_(NULL),
      listener_(listener),
      style_(style),
      min_(min),
      max_(max),
      step_(step) {
  SetFocusable(true);
}

Slider::~Slider() {
}

void Slider::NotifyValueChanged() {
  if (native_wrapper_)
    value_ = native_wrapper_->GetValue();
  if (listener_)
    listener_->SliderValueChanged(this);
}

void Slider::SetValue(double value) {
  value_ = value;
  if (native_wrapper_)
    native_wrapper_->SetValue(value);
}

////////////////////////////////////////////////////////////////////////////////
// Slider, View overrides:

void Slider::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBoundsRect(GetLocalBounds());
    native_wrapper_->GetView()->Layout();
  }
}

gfx::Size Slider::GetPreferredSize() {
  if (native_wrapper_)
    return native_wrapper_->GetPreferredSize();
  return gfx::Size();
}

void Slider::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void Slider::Focus() {
  if (native_wrapper_) {
    // Forward the focus to the wrapper if it exists.
    native_wrapper_->SetFocus();
  } else {
    // If there is no wrapper, cause the RootView to be focused so that we still
    // get keyboard messages.
    View::Focus();
  }
}

void Slider::PaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::PaintFocusBorder(canvas);
}

void Slider::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = NativeSliderWrapper::CreateWrapper(this);
    AddChildView(native_wrapper_->GetView());
    native_wrapper_->UpdateEnabled();
  }
}

std::string Slider::GetClassName() const {
  return kViewClassName;
}

NativeSliderWrapper* Slider::CreateWrapper() {
  NativeSliderWrapper* native_wrapper =
      NativeSliderWrapper::CreateWrapper(this);

  native_wrapper->UpdateEnabled();

  return native_wrapper;
}

}  // namespace views
