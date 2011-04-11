// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_NATIVE_THEME_PAINTER_H_
#define VIEWS_NATIVE_THEME_PAINTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/gfx/native_theme.h"
#include "views/painter.h"

namespace gfx {
class Canvas;
class Size;
}

namespace ui {
class Animation;
}

namespace views {

// A Painter that uses NativeTheme to implement painting and sizing.  A
// theme delegate must be given at construction time so that the appropriate
// painting and sizing can be done.
class NativeThemePainter : public Painter {
 public:
  // A delagate that supports animating transtions between different native
  // theme states.  If animation is onging, the native theme painter will
  // composite the foreground state over the backgroud state using an alpha
  // between 0 and 255 based on the current value of the animation.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Get the part that this native theme painter should draw.
    virtual gfx::NativeTheme::Part GetThemePart() const = 0;

    // Get the state of the part, along with any extra data needed for painting.
    virtual gfx::NativeTheme::State GetThemeState(
        gfx::NativeTheme::ExtraParams* params) const = 0;

    // If the native theme painter is animated, return the Animation object
    // that is controlling it.  If no animation is ongoing, NULL may be
    // returned.
    virtual ui::Animation* GetThemeAnimation() const = 0;

    // If animation is onging, this returns the background native theme state.
    virtual gfx::NativeTheme::State GetBackgroundThemeState(
        gfx::NativeTheme::ExtraParams* params) const = 0;

    // If animation is onging, this returns the foreground native theme state.
    // This state will be composited over the background using an alpha value
    // based on the current value of the animation.
    virtual gfx::NativeTheme::State GetForegroundThemeState(
        gfx::NativeTheme::ExtraParams* params) const = 0;
  };

  explicit NativeThemePainter(Delegate* delegate);

  virtual ~NativeThemePainter() {}

  // Returns the preferred size of the native part being painted.
  gfx::Size GetPreferredSize();

 private:
  // The delegate the controls the appearance of this painter.
  Delegate* delegate_;

  // Overridden from Painter:
  virtual void Paint(int w, int h, gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NativeThemePainter);
};

}  // namespace views

#endif  // VIEWS_NATIVE_THEME_PAINTER_H_
