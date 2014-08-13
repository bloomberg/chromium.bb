// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_observer.h"

#include "base/logging.h"

namespace ui {

#if defined(OS_MACOSX)
// Debugging instrumentation for crbug.com/401630.
// TODO(ccameron): remove this.
CompositorObserver::CompositorObserver() : observing_count_(0) {}
#else
CompositorObserver::CompositorObserver() {}
#endif

CompositorObserver::~CompositorObserver() {
#if defined(OS_MACOSX)
  // Debugging instrumentation for crbug.com/401630.
  // TODO(ccameron): remove this.
  CHECK_EQ(observing_count_, 0);
#endif
}

}

