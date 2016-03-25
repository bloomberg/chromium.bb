// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/native_widget_mus.h"

#include "base/macros.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace {

// Returns a small colored bitmap.
SkBitmap MakeBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8);
  bitmap.eraseColor(color);
  return bitmap;
}

// A WidgetDelegate that supplies an app icon.
class TestWidgetDelegate : public WidgetDelegateView {
 public:
  explicit TestWidgetDelegate(const SkBitmap& icon)
      : app_icon_(gfx::ImageSkia::CreateFrom1xBitmap(icon)) {}

  ~TestWidgetDelegate() override {}

  void SetIcon(const SkBitmap& icon) {
    app_icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  // views::WidgetDelegate:
  gfx::ImageSkia GetWindowAppIcon() override { return app_icon_; }

 private:
  gfx::ImageSkia app_icon_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

class NativeWidgetMusTest : public ViewsTestBase {
 public:
  NativeWidgetMusTest() {}
  ~NativeWidgetMusTest() override {}

  // Creates a test widget. Takes ownership of |delegate|.
  Widget* CreateWidget(TestWidgetDelegate* delegate) {
    Widget* widget = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(10, 20, 100, 200);
    widget->Init(params);
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMusTest);
};

// Tests that a window with an icon sets the mus::Window icon property.
TEST_F(NativeWidgetMusTest, AppIcon) {
  // Create a Widget with a bitmap as the icon.
  SkBitmap source_bitmap = MakeBitmap(SK_ColorRED);
  scoped_ptr<Widget> widget(
      CreateWidget(new TestWidgetDelegate(source_bitmap)));

  // The mus::Window has the icon property.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_TRUE(window->HasSharedProperty(
      mus::mojom::WindowManager::kWindowAppIcon_Property));

  // The icon is the expected icon.
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      mus::mojom::WindowManager::kWindowAppIcon_Property);
  EXPECT_TRUE(gfx::BitmapsAreEqual(source_bitmap, icon));
}

// Tests that a window without an icon does not set the mus::Window icon
// property.
TEST_F(NativeWidgetMusTest, NoAppIcon) {
  // Create a Widget without a special icon.
  scoped_ptr<Widget> widget(CreateWidget(nullptr));

  // The mus::Window does not have an icon property.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_FALSE(window->HasSharedProperty(
      mus::mojom::WindowManager::kWindowAppIcon_Property));
}

// Tests that changing the icon on a Widget updates the mus::Window icon
// property.
TEST_F(NativeWidgetMusTest, ChangeAppIcon) {
  // Create a Widget with an icon.
  SkBitmap bitmap1 = MakeBitmap(SK_ColorRED);
  TestWidgetDelegate* delegate = new TestWidgetDelegate(bitmap1);
  scoped_ptr<Widget> widget(CreateWidget(delegate));

  // Update the icon to a new image.
  SkBitmap bitmap2 = MakeBitmap(SK_ColorGREEN);
  delegate->SetIcon(bitmap2);
  widget->UpdateWindowIcon();

  // The window has the updated icon.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      mus::mojom::WindowManager::kWindowAppIcon_Property);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap2, icon));
}

}  // namespace
}  // namespace views
