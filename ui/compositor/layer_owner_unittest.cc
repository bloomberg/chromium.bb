// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

#include "base/test/null_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace {

// Test fixture for LayerOwner tests that require a ui::Compositor.
class LayerOwnerTestWithCompositor : public testing::Test {
 public:
  LayerOwnerTestWithCompositor();
  ~LayerOwnerTestWithCompositor() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(LayerOwnerTestWithCompositor);
};

LayerOwnerTestWithCompositor::LayerOwnerTestWithCompositor() {
}

LayerOwnerTestWithCompositor::~LayerOwnerTestWithCompositor() {
}

void LayerOwnerTestWithCompositor::SetUp() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      new base::NullTaskRunner();

  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(false);

  compositor_.reset(new ui::Compositor(gfx::kNullAcceleratedWidget,
                                       context_factory, task_runner));
}

void LayerOwnerTestWithCompositor::TearDown() {
  compositor_.reset();
  ui::TerminateContextFactoryForTests();
}

}  // namespace

TEST(LayerOwnerTest, RecreateLayerHonorsTargetVisibilityAndOpacity) {
  LayerOwner owner;
  Layer* layer = new Layer;
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);

  owner.SetLayer(layer);

  ScopedLayerAnimationSettings settings(layer->GetAnimator());
  layer->SetVisible(false);
  layer->SetOpacity(0.0f);
  EXPECT_TRUE(layer->visible());
  EXPECT_EQ(1.0f, layer->opacity());

  scoped_ptr<Layer> old_layer(owner.RecreateLayer());
  EXPECT_FALSE(owner.layer()->visible());
  EXPECT_EQ(0.0f, owner.layer()->opacity());
}

TEST(LayerOwnerTest, RecreateRootLayerWithNullCompositor) {
  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();

  EXPECT_EQ(nullptr, owner.layer()->GetCompositor());
  EXPECT_EQ(nullptr, layer_copy->GetCompositor());
}

TEST_F(LayerOwnerTestWithCompositor, RecreateRootLayerWithCompositor) {
  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);

  compositor()->SetRootLayer(layer);

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();

  EXPECT_EQ(compositor(), owner.layer()->GetCompositor());
  EXPECT_EQ(owner.layer(), compositor()->root_layer());
  EXPECT_EQ(nullptr, layer_copy->GetCompositor());
}

}  // namespace ui
