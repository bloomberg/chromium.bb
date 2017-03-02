// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_finder.h"

#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/test_server_window_delegate.h"
#include "services/ui/ws/window_finder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {

TEST(WindowFinderTest, FindDeepestVisibleWindow) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  root.set_event_targeting_policy(
      mojom::EventTargetingPolicy::DESCENDANTS_ONLY);
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.SetVisible(true);
  child2.SetBounds(gfx::Rect(15, 15, 20, 20));

  EXPECT_EQ(
      &child2,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(16, 16)).window);

  EXPECT_EQ(
      &child1,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(13, 14)).window);

  child1.set_event_targeting_policy(mojom::EventTargetingPolicy::NONE);
  EXPECT_EQ(
      nullptr,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(13, 14)).window);

  child2.set_extended_hit_test_region(gfx::Insets(10, 10, 10, 10));
  EXPECT_EQ(
      &child2,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(13, 14)).window);
}

TEST(WindowFinderTest, FindDeepestVisibleWindowNonClientArea) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));

  DeepestWindow result =
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(13, 14));
  EXPECT_EQ(&child1, result.window);
  EXPECT_FALSE(result.in_non_client_area);

  result = FindDeepestVisibleWindowForEvents(&root, gfx::Point(11, 11));
  EXPECT_EQ(&child1, result.window);
  EXPECT_FALSE(result.in_non_client_area);

  // 11, 11 is over the non-client area.
  child1.SetClientArea(gfx::Insets(2, 3, 4, 5), std::vector<gfx::Rect>());
  result = FindDeepestVisibleWindowForEvents(&root, gfx::Point(11, 11));
  EXPECT_EQ(&child1, result.window);
  EXPECT_TRUE(result.in_non_client_area);

  // 15, 15 is over the client area.
  result = FindDeepestVisibleWindowForEvents(&root, gfx::Point(15, 15));
  EXPECT_EQ(&child1, result.window);
  EXPECT_FALSE(result.in_non_client_area);

  // EventTargetingPolicy::NONE should not impact the result for the
  // non-client area.
  child1.set_event_targeting_policy(mojom::EventTargetingPolicy::NONE);
  result = FindDeepestVisibleWindowForEvents(&root, gfx::Point(11, 11));
  child1.SetClientArea(gfx::Insets(2, 3, 4, 5), std::vector<gfx::Rect>());
  EXPECT_EQ(&child1, result.window);
  EXPECT_TRUE(result.in_non_client_area);

  // EventTargetingPolicy::NONE means the client area won't be matched though.
  result = FindDeepestVisibleWindowForEvents(&root, gfx::Point(15, 15));
  EXPECT_EQ(&root, result.window);
  EXPECT_FALSE(result.in_non_client_area);
}

TEST(WindowFinderTest, FindDeepestVisibleWindowHitTestMask) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  ServerWindow child_with_mask(&window_delegate, WindowId(1, 4));
  root.Add(&child_with_mask);
  child_with_mask.SetVisible(true);
  child_with_mask.SetBounds(gfx::Rect(10, 10, 20, 20));
  child_with_mask.SetHitTestMask(gfx::Rect(2, 2, 16, 16));

  // Test a point inside the window but outside the mask.
  EXPECT_EQ(
      &root,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(11, 11)).window);

  // Test a point inside the window and inside the mask.
  EXPECT_EQ(
      &child_with_mask,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(15, 15)).window);
}

TEST(WindowFinderTest, FindDeepestVisibleWindowOverNonTarget) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  // Create two windows, |child1| and |child2|. The two overlap but |child2| is
  // not a valid event target.
  ServerWindow child1(&window_delegate, WindowId(1, 3));
  root.Add(&child1);
  child1.SetVisible(true);
  child1.SetBounds(gfx::Rect(10, 10, 20, 20));

  ServerWindow child2(&window_delegate, WindowId(1, 4));
  root.Add(&child2);
  child2.set_event_targeting_policy(mojom::EventTargetingPolicy::NONE);
  child2.SetVisible(true);
  child2.SetBounds(gfx::Rect(15, 15, 20, 20));

  // 16, 16 is over |child2| and |child1|, but as |child2| isn't a valid event
  // target |child1| should be picked.
  EXPECT_EQ(
      &child1,
      FindDeepestVisibleWindowForEvents(&root, gfx::Point(16, 16)).window);
}

TEST(WindowFinderTest, NonClientPreferredOverChild) {
  TestServerWindowDelegate window_delegate;
  ServerWindow root(&window_delegate, WindowId(1, 2));
  window_delegate.set_root_window(&root);
  root.SetVisible(true);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  // Create two windows, |child| and |child_child|; |child| is a child of the
  // root and |child_child| and child of |child|. All share the same size with
  // |child| having a non-client area.
  ServerWindow child(&window_delegate, WindowId(1, 3));
  root.Add(&child);
  child.SetVisible(true);
  child.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.SetClientArea(gfx::Insets(2, 3, 4, 5), std::vector<gfx::Rect>());

  ServerWindow child_child(&window_delegate, WindowId(1, 4));
  child.Add(&child_child);
  child_child.SetVisible(true);
  child_child.SetBounds(gfx::Rect(0, 0, 100, 100));

  // |child| was should be returned as the event is over the non-client area.
  EXPECT_EQ(&child,
            FindDeepestVisibleWindowForEvents(&root, gfx::Point(1, 1)).window);
}

}  // namespace ws
}  // namespace ui
