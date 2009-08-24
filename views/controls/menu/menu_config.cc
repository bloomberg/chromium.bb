// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_config.h"

#include "build/build_config.h"

namespace views {

static MenuConfig* config_instance = NULL;

void MenuConfig::Reset() {
  delete config_instance;
  config_instance = NULL;
}

// static
const MenuConfig& MenuConfig::instance() {
  if (!config_instance)
    config_instance = Create();
  return *config_instance;
}

}  // namespace views
