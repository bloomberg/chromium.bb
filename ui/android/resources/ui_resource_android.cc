// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/ui_resource_android.h"

#include "base/logging.h"
#include "ui/android/resources/ui_resource_provider.h"

namespace ui {

scoped_ptr<UIResourceAndroid> UIResourceAndroid::CreateFromJavaBitmap(
    ui::UIResourceProvider* provider,
    const gfx::JavaBitmap& java_bitmap) {
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap);
  skbitmap.setImmutable();

  return make_scoped_ptr(new UIResourceAndroid(provider, skbitmap));
}

UIResourceAndroid::~UIResourceAndroid() {
  if (id_ && provider_)
    provider_->DeleteUIResource(id_);
}

cc::UIResourceBitmap UIResourceAndroid::GetBitmap(cc::UIResourceId uid,
                                                  bool resource_lost) {
  DCHECK(!bitmap_.empty());
  return cc::UIResourceBitmap(bitmap_);
}

cc::UIResourceId UIResourceAndroid::id() {
  if (id_)
    return id_;
  if (!provider_ || bitmap_.empty())
    return 0;
  id_ = provider_->CreateUIResource(this);
  return id_;
}

void UIResourceAndroid::UIResourceIsInvalid() {
  id_ = 0;
}

UIResourceAndroid::UIResourceAndroid(ui::UIResourceProvider* provider,
                                     const SkBitmap& skbitmap)
    : provider_(provider), bitmap_(skbitmap), id_(0) {
}

}  // namespace ui
