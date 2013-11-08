// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/config.h"

#include "tools/gn/err.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/scheduler.h"

Config::Config(const Settings* settings, const Label& label)
    : Item(settings, label) {
}

Config::~Config() {
}

Config* Config::AsConfig() {
  return this;
}

const Config* Config::AsConfig() const {
  return this;
}
