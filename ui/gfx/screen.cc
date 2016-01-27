// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/gfx/screen.h"

namespace gfx {

namespace {

Screen* g_screen;

}  // namespace

Screen::Screen() {
}

Screen::~Screen() {
}

// static
Screen* Screen::GetScreen() {
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // TODO(scottmg): https://crbug.com/558054
  if (!g_screen)
    g_screen = CreateNativeScreen();
#endif
  return g_screen;
}

// static
void Screen::SetScreenInstance(Screen* instance) {
  g_screen = instance;
}

}  // namespace gfx
