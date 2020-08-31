// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/aura_components.h"

namespace chromecast {

AuraComponents::AuraComponents(CastWindowManager* cast_window_manager)
    : media_overlay_(nullptr) {}

AuraComponents::~AuraComponents() = default;

}  // namespace chromecast
