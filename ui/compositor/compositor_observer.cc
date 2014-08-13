// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_observer.h"

#include "base/logging.h"

namespace ui {

#if DCHECK_IS_ON
CompositorObserver::CompositorObserver() : observing_count_(0) {}
#else
CompositorObserver::CompositorObserver() {}
#endif

CompositorObserver::~CompositorObserver() {
  // TODO(ccameron): Make this check not fire on non-Mac platforms.
  // http://crbug.com/403011
#if defined(OS_MACOSX)
  DCHECK_EQ(observing_count_, 0);
#endif
}

}

