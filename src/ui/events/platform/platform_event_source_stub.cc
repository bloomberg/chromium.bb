// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/platform_event_source.h"

namespace ui {

std::unique_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return nullptr;
}

}  // namespace ui
