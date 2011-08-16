// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SURFACE_ACCELERATED_SURFACE_LINUX_H_
#define UI_GFX_SURFACE_ACCELERATED_SURFACE_LINUX_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/surface_export.h"

// The GL context associated with the surface must be current when
// an instance is created or destroyed.
class SURFACE_EXPORT AcceleratedSurface
    : public base::RefCounted<AcceleratedSurface> {
 public:
  AcceleratedSurface(const gfx::Size& size);
  const gfx::Size& size() const { return size_; }
  uint32 pixmap() const { return pixmap_; }
  uint32 texture() const { return texture_; }

 private:
  ~AcceleratedSurface();

  gfx::Size size_;
  void* image_;
  uint32 pixmap_;
  uint32 texture_;

  friend class base::RefCounted<AcceleratedSurface>;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurface);
};

#endif  // UI_GFX_SURFACE_ACCELERATED_SURFACE_LINUX_H_
