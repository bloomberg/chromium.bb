// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_UI_RESOURCE_ANDROID_H_
#define UI_ANDROID_RESOURCES_UI_RESOURCE_ANDROID_H_

#include "base/basictypes.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/ui_resource_client_android.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/android/java_bitmap.h"

namespace ui {

class UIResourceProvider;

class UI_ANDROID_EXPORT UIResourceAndroid : public ui::UIResourceClientAndroid {
 public:
  static scoped_ptr<UIResourceAndroid> CreateFromJavaBitmap(
      ui::UIResourceProvider* provider,
      const gfx::JavaBitmap& java_bitmap);

  ~UIResourceAndroid() override;

  // cc::UIResourceClient implementation.
  cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                 bool resource_lost) override;

  // ui::UIResourceClientAndroid implementation.
  void UIResourceIsInvalid() override;

  cc::UIResourceId id();

 private:
  UIResourceAndroid(ui::UIResourceProvider* provider, const SkBitmap& skbitmap);

  ui::UIResourceProvider* provider_;
  SkBitmap bitmap_;
  cc::UIResourceId id_;

  DISALLOW_COPY_AND_ASSIGN(UIResourceAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_UI_RESOURCE_ANDROID_H_
