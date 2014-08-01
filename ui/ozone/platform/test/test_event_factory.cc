// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_event_factory.h"

#include "ui/events/platform/platform_event_source.h"

namespace ui {

class TestPlatformEventSource : public PlatformEventSource {
 public:
  TestPlatformEventSource() {}
};

TestEventFactory::TestEventFactory() {
  // This unbreaks tests that create their own.
  if (!PlatformEventSource::GetInstance())
    event_source_.reset(new TestPlatformEventSource);
}

TestEventFactory::~TestEventFactory() {}

void TestEventFactory::WarpCursorTo(gfx::AcceleratedWidget widget,
                                    const gfx::PointF& location) {}

}  // namespace ui
