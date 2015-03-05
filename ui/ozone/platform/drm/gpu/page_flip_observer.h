// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_OBSERVER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_OBSERVER_H_

namespace ui {

class PageFlipObserver {
 public:
  virtual ~PageFlipObserver() {}

  virtual void OnPageFlipEvent() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_OBSERVER_H_
