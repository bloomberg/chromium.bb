// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace ui {

class PageFlipRequest : public base::RefCounted<PageFlipRequest> {
 public:
  PageFlipRequest(int crtc_count, const base::Closure& callback);

  void Signal();

 private:
  friend class base::RefCounted<PageFlipRequest>;
  ~PageFlipRequest();

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipRequest);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_
