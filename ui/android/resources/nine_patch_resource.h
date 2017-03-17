// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_
#define UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_

#include "ui/android/resources/resource.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/insets_f.h"

namespace ui {

class UI_ANDROID_EXPORT NinePatchResource final : public Resource {
 public:
  static NinePatchResource* From(Resource* resource);

  NinePatchResource(gfx::Rect padding, gfx::Rect aperture);
  ~NinePatchResource() override;

  std::unique_ptr<Resource> CreateForCopy() override;

  gfx::Rect Border(const gfx::Size& bounds) const;
  gfx::Rect Border(const gfx::Size& bounds, const gfx::InsetsF& scale) const;

  gfx::Rect padding() const { return padding_; }
  gfx::Rect aperture() const { return aperture_; }

 private:
  const gfx::Rect padding_;
  const gfx::Rect aperture_;
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_
