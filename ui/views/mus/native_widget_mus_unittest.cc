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
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
namespace {

// Returns a small colored bitmap.
SkBitmap MakeBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8);
  bitmap.eraseColor(color);
  return bitmap;
}

// An observer that tracks widget activation changes.
class WidgetActivationObserver : public WidgetObserver {
 public:
  explicit WidgetActivationObserver(Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }

  ~WidgetActivationObserver() override {
    widget_->RemoveObserver(this);
  }

  const std::vector<bool>& changes() const { return changes_; }

  // WidgetObserver:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    ASSERT_EQ(widget_, widget);
    changes_.push_back(active);
  }

 private:
  Widget* widget_;
  std::vector<bool> changes_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationObserver);
};

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
  std::unique_ptr<Widget> CreateWidget(TestWidgetDelegate* delegate) {
    std::unique_ptr<Widget> widget(new Widget());
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

// Tests communication of activation and focus between Widget and
// NativeWidgetMus.
TEST_F(NativeWidgetMusTest, OnActivationChanged) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  // Track activation, focus and blur events.
  WidgetActivationObserver activation_observer(widget.get());
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);

  // Deactivate the Widget, which deactivates the NativeWidgetMus.
  widget->Deactivate();

  // The widget is blurred and deactivated.
  ASSERT_EQ(1u, focus_listener.focus_changes().size());
  EXPECT_EQ(nullptr, focus_listener.focus_changes()[0]);
  ASSERT_EQ(1u, activation_observer.changes().size());
  EXPECT_EQ(false, activation_observer.changes()[0]);

  // Re-activate the Widget, which actives the NativeWidgetMus.
  widget->Activate();

  // The widget is focused and activated.
  ASSERT_EQ(2u, focus_listener.focus_changes().size());
  EXPECT_EQ(widget->GetNativeView(), focus_listener.focus_changes()[1]);
  ASSERT_EQ(2u, activation_observer.changes().size());
  EXPECT_TRUE(activation_observer.changes()[1]);

  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

// Tests that a window with an icon sets the mus::Window icon property.
TEST_F(NativeWidgetMusTest, AppIcon) {
  // Create a Widget with a bitmap as the icon.
  SkBitmap source_bitmap = MakeBitmap(SK_ColorRED);
  std::unique_ptr<Widget> widget(
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
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));

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
  std::unique_ptr<Widget> widget(CreateWidget(delegate));

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

TEST_F(NativeWidgetMusTest, ValidLayerTree) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  View* content = new View;
  content->SetPaintToLayer(true);
  widget->GetContentsView()->AddChildView(content);
  EXPECT_TRUE(widget->GetNativeWindow()->layer()->Contains(content->layer()));
}

// Tests that the internal name is propagated from the Widget to the
// mus::Window.
TEST_F(NativeWidgetMusTest, GetName) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "MyWidget";
  widget.Init(params);
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget.native_widget_private())->window();
  EXPECT_EQ("MyWidget", window->GetName());
}

}  // namespace
}  // namespace views
