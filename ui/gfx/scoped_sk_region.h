// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GFX_SCOPED_REGION_GFX_H_
#define UI_BASE_GFX_SCOPED_REGION_GFX_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace gfx {

// Wraps a Region. This class provides the same methods as ScopedGDIObject in
// scoped_handle_win.
class ScopedSkRegion {
 public:
  ScopedSkRegion() {}
  explicit ScopedSkRegion(SkRegion* region) { region_.reset(region); }

  ~ScopedSkRegion() {
  }

  void Set(SkRegion* region) {
    region_.reset(region);
  }

  SkRegion* Get() {
    return region_.get();
  }

  SkRegion* release() {
    return region_.release();
  }

 private:
  scoped_ptr<SkRegion> region_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSkRegion);
};

}  // namespace gfx

#endif  // UI_BASE_GFX_SCOPED_REGION_GFX_H_
