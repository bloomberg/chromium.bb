// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "views/controls/slider/native_slider_gtk.h"

#include "ui/gfx/gtk_util.h"
#include "views/controls/slider/slider.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeSliderGtk, public:

NativeSliderGtk::NativeSliderGtk(Slider* slider)
    : slider_(slider) {
}

NativeSliderGtk::~NativeSliderGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeSliderGtk, NativeSliderWrapper implementation:

void NativeSliderGtk::UpdateEnabled() {
  if (!native_view())
    return;
  SetEnabled(slider_->IsEnabled());
}

double NativeSliderGtk::GetValue() {
  if (!native_view())
    return 0;
  return gtk_range_get_value(GTK_RANGE(native_view()));
}

void NativeSliderGtk::SetValue(double value) {
  if (!native_view())
    return;
  gtk_range_set_value(GTK_RANGE(native_view()), value);
}

void NativeSliderGtk::SetFocus() {
  Focus();
}

gfx::Size NativeSliderGtk::GetPreferredSize() {
  if (!native_view())
    return gfx::Size();

  if (preferred_size_.IsEmpty()) {
    GtkRequisition size_request = { 0, 0 };
    gtk_widget_size_request(native_view(), &size_request);
    preferred_size_.SetSize(size_request.width, size_request.height);
  }
  return preferred_size_;
}

View* NativeSliderGtk::GetView() {
  return this;
}

gfx::NativeView NativeSliderGtk::GetTestingHandle() const {
  return native_view();
}

// static
gboolean NativeSliderGtk::OnValueChangedHandler(GtkWidget* entry,
                                                NativeSliderGtk* slider) {
  return slider->OnValueChanged();
}

gboolean NativeSliderGtk::OnValueChanged() {
  slider_->NotifyValueChanged();
  return false;
}
////////////////////////////////////////////////////////////////////////////////
// NativeSliderGtk, NativeControlGtk overrides:

void NativeSliderGtk::CreateNativeControl() {
  GtkWidget* widget;
  if (slider_->style() & Slider::STYLE_VERTICAL)
    widget = gtk_vscale_new_with_range(slider_->min(),
                                       slider_->max(),
                                       slider_->step());
  else
    widget = gtk_hscale_new_with_range(slider_->min(),
                                       slider_->max(),
                                       slider_->step());
  NativeControlCreated(widget);

  bool drawvalue = slider_->style() & Slider::STYLE_DRAW_VALUE;
  gtk_scale_set_draw_value(GTK_SCALE(native_view()), drawvalue);

  int digits = 0;
  if (slider_->style() & Slider::STYLE_ONE_DIGIT)
    digits = 1;
  else if (slider_->style() & Slider::STYLE_TWO_DIGITS)
    digits = 2;
  gtk_scale_set_digits(GTK_SCALE(native_view()), digits);

  if (slider_->style() & Slider::STYLE_UPDATE_ON_RELEASE)
    gtk_range_set_update_policy(GTK_RANGE(native_view()),
                                GTK_UPDATE_DISCONTINUOUS);
}

void NativeSliderGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);
  g_signal_connect(widget, "value_changed",
                   G_CALLBACK(OnValueChangedHandler), this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeSliderWrapper, public:

// static
NativeSliderWrapper* NativeSliderWrapper::CreateWrapper(Slider* field) {
  return new NativeSliderGtk(field);
}

}  // namespace views
