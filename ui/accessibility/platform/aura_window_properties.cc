// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/aura_window_properties.h"

#include "ui/base/class_property.h"

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(AXTreeIDRegistry::AXTreeID,
                             kChildAXTreeID,
                             AXTreeIDRegistry::kNoAXTreeID);

}  // namespace ui
