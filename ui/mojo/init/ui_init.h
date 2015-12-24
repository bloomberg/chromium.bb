// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MOJO_INIT_UI_INIT_H_
#define UI_MOJO_INIT_UI_INIT_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"

namespace gfx {
class Display;
class Screen;
}

namespace ui {
class GestureConfiguration;

namespace mojo {

class GestureConfigurationMojo;

// UIInit configures any state needed by apps that make use of ui classes (not
// including aura).
class UIInit {
 public:
  explicit UIInit(const std::vector<gfx::Display>& displays);
  ~UIInit();

 private:
  scoped_ptr<gfx::Screen> screen_;
#if defined(OS_ANDROID)
  scoped_ptr<GestureConfigurationMojo> gesture_configuration_;
#endif

  DISALLOW_COPY_AND_ASSIGN(UIInit);
};

}  // namespace mojo
}  // namespace ui

#endif  // UI_MOJO_INIT_UI_INIT_H_
