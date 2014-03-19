// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace wm {

typedef aura::test::AuraTestBase WindowUtilTest;

// Test if the recreate layers does not recreate layers that have
// already been acquired.
TEST_F(WindowUtilTest, RecreateLayers) {
  scoped_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, NULL));
  window1->SetName("1");
  scoped_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));
  window11->SetName("11");
  scoped_ptr<aura::Window> window12(
      aura::test::CreateTestWindowWithId(2, window1.get()));
  window12->SetName("12");

  ASSERT_EQ(2u, window1->layer()->children().size());
  EXPECT_EQ("11 1", window1->layer()->children()[0]->name());
  EXPECT_EQ("12 2", window1->layer()->children()[1]->name());

  scoped_ptr<ui::Layer> acquired(window11->AcquireLayer());
  EXPECT_TRUE(acquired.get());
  EXPECT_EQ(acquired.get(), window11->layer());

  scoped_ptr<ui::LayerTreeOwner> tree =
      wm::RecreateLayers(window1.get());

  // The detached layer should not have the layer that has
  // already been detached.
  ASSERT_EQ(1u, tree->root()->children().size());
  EXPECT_EQ("12 2", tree->root()->children()[0]->name());

  // The original window should have both.
  ASSERT_EQ(2u, window1->layer()->children().size());
  EXPECT_EQ("11 1", window1->layer()->children()[0]->name());
  EXPECT_EQ("12 2", window1->layer()->children()[1]->name());
}
}  // namespace wm
