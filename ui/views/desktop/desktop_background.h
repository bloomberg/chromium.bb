// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DESKTOP_DESKTOP_BACKGROUND_H_
#define UI_VIEWS_DESKTOP_DESKTOP_BACKGROUND_H_

#include "base/compiler_specific.h"
#include "views/background.h"

namespace views {
namespace desktop {

class DesktopBackground : public Background {
 public:
  DesktopBackground();
  virtual ~DesktopBackground();

 private:
  // Overridden from Background:
  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackground);
};

}  // namespace desktop
}  // namespace views

#endif  // UI_VIEWS_DESKTOP_DESKTOP_BACKGROUND_H_
