// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

#include "base/barrier_closure.h"

namespace ui {

PageFlipRequest::PageFlipRequest(int crtc_count, const base::Closure& callback)
    : callback_(base::BarrierClosure(crtc_count, callback)) {
}

PageFlipRequest::~PageFlipRequest() {
}

void PageFlipRequest::Signal() {
  callback_.Run();
}

}  // namespace ui
