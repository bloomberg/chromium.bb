// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PluginsChangedObserver.h"

#include "core/page/Page.h"

namespace blink {

PluginsChangedObserver::PluginsChangedObserver(Page* page) {
  if (page)
    page->RegisterPluginsChangedObserver(this);
}

}  // namespace blink
