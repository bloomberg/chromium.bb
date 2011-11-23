// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
#define VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
#pragma once

#include "base/compiler_specific.h"
#include "views/view.h"

namespace views {

class MenuSeparator : public View {
 public:
  MenuSeparator() {}

  // View overrides.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuSeparator);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_SEPARATOR_H_
