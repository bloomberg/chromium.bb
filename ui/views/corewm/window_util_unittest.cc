// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/window_util.h"

#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

class WindowUtilTest : public aura::test::AuraTestBase {
 public:
  WindowUtilTest() {
  }

  virtual ~WindowUtilTest() {
  }

  // Creates a widget of TYPE_CONTROL.
  // The caller takes ownership of the returned widget.
  views::Widget* CreateControlWidget(
      aura::Window* parent,
      const gfx::Rect& bounds) const WARN_UNUSED_RESULT {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.parent = parent;
    params.bounds = bounds;
    views::Widget* widget = new views::Widget();
    widget->Init(params);
    return widget;
  }

  // Returns a view with a layer with the passed in |bounds| and |layer_name|.
  // The caller takes ownership of the returned view.
  views::View* CreateViewWithLayer(
      const gfx::Rect& bounds,
      const char* layer_name) const WARN_UNUSED_RESULT {
    views::View* view = new views::View();
    view->SetBoundsRect(bounds);
    view->SetPaintToLayer(true);
    view->layer()->set_name(layer_name);
    return view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowUtilTest);
};

// Test that RecreateWindowLayers() recreates the layers for all child windows
// and all child views and that the z-order of the recreated layers matches that
// of the original layers.
// Test hierarchy:
// w1
// +-- v1
// +-- v2 (no layer)
//     +-- v3 (no layer)
//     +-- v4
// +-- w2
//     +-- v5
//         +-- v6
// +-- v7
TEST_F(WindowUtilTest, RecreateWindowLayers) {
  views::Widget* w1 = CreateControlWidget(root_window(),
                                          gfx::Rect(0, 0, 100, 100));
  w1->GetNativeView()->layer()->set_name("w1");

  views::View* v2 = new views::View();
  v2->SetBounds(0, 1, 100, 101);
  views::View* v3 = new views::View();
  v3->SetBounds(0, 2, 100, 102);
  views::View* w2_host_view = new views::View();

  w1->GetRootView()->AddChildView(CreateViewWithLayer(
      gfx::Rect(0, 3, 100, 103), "v1"));
  w1->GetRootView()->AddChildView(v2);
  v2->AddChildView(v3);
  v2->AddChildView(CreateViewWithLayer(gfx::Rect(0, 4, 100, 104), "v4"));

  w1->GetRootView()->AddChildView(w2_host_view);
  w1->GetRootView()->AddChildView(CreateViewWithLayer(
      gfx::Rect(0, 4, 100, 104), "v7"));

  views::Widget* w2 = CreateControlWidget(w1->GetNativeView(),
                                          gfx::Rect(0, 5, 100, 105));
  w2->GetNativeView()->layer()->set_name("w2");
  w2->GetNativeView()->SetProperty(views::kHostViewKey, w2_host_view);

  views::View* v5 = CreateViewWithLayer(gfx::Rect(0, 6, 100, 106), "v5");
  w2->GetRootView()->AddChildView(v5);
  v5->AddChildView(CreateViewWithLayer(gfx::Rect(0, 7, 100, 107), "v6"));

  // Test the initial order of the layers.
  ui::Layer* w1_layer = w1->GetNativeView()->layer();
  ASSERT_EQ("w1", w1_layer->name());
  ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_layer));
  ui::Layer* w2_layer = w1_layer->children()[2];
  ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_layer));
  ui::Layer* v5_layer = w2_layer->children()[0];
  ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_layer));

  w1_layer = views::corewm::RecreateWindowLayers(w1->GetNativeView(), false);

  // The order of the layers returned by RecreateWindowLayers() should match the
  // order of the layers prior to calling RecreateWindowLayers().
  ASSERT_EQ("w1", w1_layer->name());
  ASSERT_EQ("v1 v4 w2 v7", ui::test::ChildLayerNamesAsString(*w1_layer));
  w2_layer = w1_layer->children()[2];
  ASSERT_EQ("v5", ui::test::ChildLayerNamesAsString(*w2_layer));
  v5_layer = w2_layer->children()[0];
  ASSERT_EQ("v6", ui::test::ChildLayerNamesAsString(*v5_layer));

  views::corewm::DeepDeleteLayers(w1_layer);
  // The views and the widgets are destroyed when AuraTestHelper::TearDown()
  // destroys root_window().
}
