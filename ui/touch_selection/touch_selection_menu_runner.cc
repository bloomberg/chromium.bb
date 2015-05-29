// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace ui {
namespace {

TouchSelectionMenuRunner* g_touch_selection_menu_runner = nullptr;

}  // namespace

TouchSelectionMenuRunner::~TouchSelectionMenuRunner() {
  g_touch_selection_menu_runner = nullptr;
}

TouchSelectionMenuRunner* TouchSelectionMenuRunner::GetInstance() {
  return g_touch_selection_menu_runner;
}

TouchSelectionMenuRunner::TouchSelectionMenuRunner() {
  // TODO(mohsen): Ideally we should DCHECK that |g_touch_selection_menu_runner|
  // is not set here, in order to make sure we don't create multiple menu
  // runners accidentally. Currently, this is not possible because we can have
  // multiple ViewsDelegate's at the same time which should not happen. See
  // crbug.com/492991.
  g_touch_selection_menu_runner = this;
}

}  // namespace ui
