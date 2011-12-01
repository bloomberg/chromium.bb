// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_NATIVE_THEME_DELEGATE_H_
#define UI_VIEWS_NATIVE_THEME_DELEGATE_H_
#pragma once

#include "ui/gfx/native_theme.h"
#include "ui/gfx/rect.h"
#include "ui/views/views_export.h"

namespace views {

// A delagate that supports animating transtions between different native
// theme states.  This delegate can be used to control a native theme Border
// or Painter object.
//
// If animation is onging, the native theme border or painter will
// composite the foreground state over the backgroud state using an alpha
// between 0 and 255 based on the current value of the animation.
class VIEWS_EXPORT NativeThemeDelegate {
 public:
  virtual ~NativeThemeDelegate() {}

  // Get the native theme part that should be drawn.
  virtual gfx::NativeTheme::Part GetThemePart() const = 0;

  // Get the rectangle that should be painted.
  virtual gfx::Rect GetThemePaintRect() const = 0;

  // Get the state of the part, along with any extra data needed for drawing.
  virtual gfx::NativeTheme::State GetThemeState(
      gfx::NativeTheme::ExtraParams* params) const = 0;

  // If the native theme drawign should be animated, return the Animation object
  // that controlls it.  If no animation is ongoing, NULL may be returned.
  virtual const ui::Animation* GetThemeAnimation() const = 0;

  // If animation is onging, this returns the background native theme state.
  virtual gfx::NativeTheme::State GetBackgroundThemeState(
      gfx::NativeTheme::ExtraParams* params) const = 0;

  // If animation is onging, this returns the foreground native theme state.
  // This state will be composited over the background using an alpha value
  // based on the current value of the animation.
  virtual gfx::NativeTheme::State GetForegroundThemeState(
      gfx::NativeTheme::ExtraParams* params) const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_NATIVE_THEME_DELEGATE_H_
