// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_GTK_H_
#define VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "views/controls/native_control_gtk.h"
#include "views/controls/slider/native_slider_wrapper.h"

namespace views {

class NativeSliderGtk : public NativeControlGtk,
                        public NativeSliderWrapper {
 public:
  explicit NativeSliderGtk(Slider* parent);
  ~NativeSliderGtk();

  // Overridden from NativeSliderWrapper:
  virtual void UpdateEnabled();
  virtual double GetValue();
  virtual void SetValue(double value);
  virtual void SetFocus();
  virtual gfx::Size GetPreferredSize();
  virtual View* GetView();
  virtual gfx::NativeView GetTestingHandle() const;

  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

 private:
  // The slider we are bound to.
  Slider* slider_;

  // The preferred size from the last size_request. See
  // NativeButtonGtk::preferred_size_ for more detail why we need this.
  gfx::Size preferred_size_;

  // Callback when the slider value changes.
  static gboolean OnValueChangedHandler(GtkWidget* entry,
                                        NativeSliderGtk* slider);
  gboolean OnValueChanged();

  DISALLOW_COPY_AND_ASSIGN(NativeSliderGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_GTK_H_
