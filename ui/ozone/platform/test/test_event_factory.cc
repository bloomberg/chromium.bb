// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_event_factory.h"

namespace ui {

TestEventFactory::TestEventFactory() {}

TestEventFactory::~TestEventFactory() {}

void TestEventFactory::WarpCursorTo(gfx::AcceleratedWidget widget,
                                    const gfx::PointF& location) {}

}  // namespace ui
