// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_WRAPPER_H_
#define VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_WRAPPER_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace views {

class Slider;
class View;

// An interface implemented by an object that provides a platform-native slider.
class NativeSliderWrapper {
 public:
  // The Slider calls this when it is destroyed to clean up the wrapper object.
  virtual ~NativeSliderWrapper() {}

  // Updates the enabled state of the native slider.
  virtual void UpdateEnabled() = 0;

  // Gets the value of the slider.
  virtual double GetValue() = 0;

  // Sets the value of the slider.
  virtual void SetValue(double value) = 0;

  // Sets the focus to the slider.
  virtual void SetFocus() = 0;

  // Returns the preferred size of the combobox.
  virtual gfx::Size GetPreferredSize() = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  // Creates an appropriate NativeSliderWrapper for the platform.
  static NativeSliderWrapper* CreateWrapper(Slider* slider);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SLIDER_NATIVE_SLIDER_WRAPPER_H_
