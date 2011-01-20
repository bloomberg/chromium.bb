// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GTK_SCOPED_HANDLE_GTK_H_
#define UI_BASE_GTK_SCOPED_HANDLE_GTK_H_
#pragma once

#include <gdk/gdk.h>

namespace ui {

// Wraps a GdkRegion. This class provides the same methods as ScopedGDIObject in
// scoped_handle_win.
class ScopedRegion {
 public:
  ScopedRegion() : region_(NULL) {}
  explicit ScopedRegion(GdkRegion* region) : region_(region) {}

  ~ScopedRegion() {
    Close();
  }

  void Set(GdkRegion* region) {
    Close();

    region_ = region;
  }

  GdkRegion* Get() {
    return region_;
  }

  GdkRegion* release() {
    GdkRegion* region = region_;
    region_ = NULL;
    return region;
  }

 private:
  void Close() {
    if (region_) {
      gdk_region_destroy(region_);
      region_ = NULL;
    }
  }

  GdkRegion* region_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRegion);
};

}  // namespace ui

#endif  // UI_BASE_GTK_SCOPED_HANDLE_GTK_H_
