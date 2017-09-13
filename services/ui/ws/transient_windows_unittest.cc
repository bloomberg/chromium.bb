// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {

namespace {

class TestTransientWindowObserver : public ServerWindowObserver {
 public:
  TestTransientWindowObserver() : add_count_(0), remove_count_(0) {}

  ~TestTransientWindowObserver() override {}

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }

  // TransientWindowObserver overrides:
  void OnTransientWindowAdded(ServerWindow* window,
                              ServerWindow* transient) override {
    add_count_++;
  }
  void OnTransientWindowRemoved(ServerWindow* window,
                                ServerWindow* transient) override {
    remove_count_++;
  }

 private:
  int add_count_;
  int remove_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTransientWindowObserver);
};

ServerWindow* CreateTestWindow(TestServerWindowDelegate* delegate,
                               const WindowId& window_id,
                               ServerWindow* parent) {
  ServerWindow* window = new ServerWindow(delegate, window_id);
  window->SetVisible(true);
  if (parent)
    parent->Add(window);
  else
    delegate->set_root_window(window);
  return window;
}

std::string ChildWindowIDsAsString(ServerWindow* parent) {
  std::string result;
  for (auto i = parent->children().begin(); i != parent->children().end();
       ++i) {
    if (!result.empty())
      result += " ";
    WindowId id = (*i)->id();
    result += base::IntToString((id.client_id << 16) | id.window_id);
  }
  return result;
}

}  // namespace

class TransientWindowsTest : public testing::Test {
 public:
  TransientWindowsTest() {}
  ~TransientWindowsTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientWindowsTest);
};

TEST_F(TransientWindowsTest, TransientChildren) {
  TestServerWindowDelegate server_window_delegate;

  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(1, 0), nullptr));
  std::unique_ptr<ServerWindow> w1(
      CreateTestWindow(&server_window_delegate, WindowId(1, 1), parent.get()));
  std::unique_ptr<ServerWindow> w3(
      CreateTestWindow(&server_window_delegate, WindowId(1, 2), parent.get()));

  ServerWindow* w2 =
      CreateTestWindow(&server_window_delegate, WindowId(1, 3), parent.get());

  // w2 is now owned by w1.
  w1->AddTransientWindow(w2);
  // Stack w1 at the top (end), this should force w2 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = nullptr;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
}

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowsTest, TransientChildrenGroupAbove) {
  TestServerWindowDelegate server_window_delegate;

  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(0, 1), nullptr));
  std::unique_ptr<ServerWindow> w2(
      CreateTestWindow(&server_window_delegate, WindowId(0, 2), parent.get()));

  ServerWindow* w21 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 21), parent.get());
  std::unique_ptr<ServerWindow> w3(
      CreateTestWindow(&server_window_delegate, WindowId(0, 3), parent.get()));

  ServerWindow* w31 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 31), parent.get());
  ServerWindow* w311 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 311), parent.get());
  ServerWindow* w312 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 312), parent.get());
  ServerWindow* w313 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 313), parent.get());
  ServerWindow* w32 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 32), parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w21 is now owned by w2.
  w2->AddTransientWindow(w21);
  // w31 is now owned by w3.
  w3->AddTransientWindow(w31);
  // w32 is now owned by w3.
  w3->AddTransientWindow(w32);
  // w311 is now owned by w31.
  w31->AddTransientWindow(w311);
  // w312 is now owned by w31.
  w31->AddTransientWindow(w312);
  // w313 is now owned by w31.
  w31->AddTransientWindow(w313);
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  // Stack w2 at the top (end), this should force w11 to be last (on top of w2).
  parent->StackChildAtTop(w2.get());
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 31 311 312 313 32 2 21", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '32' would be following '31'.
  parent->StackChildAtTop(w3.get());
  EXPECT_EQ(w32, parent->children().back());
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w3.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 31 311 312 313 32 2 21", ChildWindowIDsAsString(parent.get()));

  w31->Reorder(w2.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w32, parent->children().back());
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  w31->Reorder(w32, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w313, parent->children().back());
  EXPECT_EQ("2 21 3 32 31 311 312 313", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w31, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 311 312 313 2 21", ChildWindowIDsAsString(parent.get()));

  w313->Reorder(w31, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent above its transient child.
  w31->Reorder(w311, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '32' would be following '31'.
  w3->Reorder(w2.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w312, parent->children().back());
  EXPECT_EQ("2 21 3 32 31 313 311 312", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w313, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));
}

TEST_F(TransientWindowsTest, TransienChildGroupBelow) {
  TestServerWindowDelegate server_window_delegate;

  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&server_window_delegate, WindowId(0, 1), nullptr));
  std::unique_ptr<ServerWindow> w2(
      CreateTestWindow(&server_window_delegate, WindowId(0, 2), parent.get()));

  ServerWindow* w21 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 21), parent.get());
  std::unique_ptr<ServerWindow> w3(
      CreateTestWindow(&server_window_delegate, WindowId(0, 3), parent.get()));

  ServerWindow* w31 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 31), parent.get());
  ServerWindow* w311 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 311), parent.get());
  ServerWindow* w312 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 312), parent.get());
  ServerWindow* w313 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 313), parent.get());
  ServerWindow* w32 =
      CreateTestWindow(&server_window_delegate, WindowId(0, 32), parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w21 is now owned by w2.
  w2->AddTransientWindow(w21);
  // w31 is now owned by w3.
  w3->AddTransientWindow(w31);
  // w32 is now owned by w3.
  w3->AddTransientWindow(w32);
  // w311 is now owned by w31.
  w31->AddTransientWindow(w311);
  // w312 is now owned by w31.
  w31->AddTransientWindow(w312);
  // w313 is now owned by w31.
  w31->AddTransientWindow(w313);
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  // Stack w3 at the bottom, this should force w21 to be last (on top of w2).
  // This also tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '32' would be following '31'.
  parent->StackChildAtBottom(w3.get());
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 31 311 312 313 32 2 21", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAtBottom(w2.get());
  EXPECT_EQ(w32, parent->children().back());
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  w31->Reorder(w2.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 31 311 312 313 32 2 21", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w3.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w32, parent->children().back());
  EXPECT_EQ("2 21 3 31 311 312 313 32", ChildWindowIDsAsString(parent.get()));

  w32->Reorder(w31, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w313, parent->children().back());
  EXPECT_EQ("2 21 3 32 31 311 312 313", ChildWindowIDsAsString(parent.get()));

  w31->Reorder(w21, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 311 312 313 2 21", ChildWindowIDsAsString(parent.get()));

  w313->Reorder(w311, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent below its transient child.
  w31->Reorder(w311, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));

  w2->Reorder(w3.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w312, parent->children().back());
  EXPECT_EQ("2 21 3 32 31 313 311 312", ChildWindowIDsAsString(parent.get()));

  w313->Reorder(w21, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w21, parent->children().back());
  EXPECT_EQ("3 32 31 313 311 312 2 21", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient windows are stacked properly when created.
TEST_F(TransientWindowsTest, StackUponCreation) {
  TestServerWindowDelegate delegate;
  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(0, 1), nullptr));
  std::unique_ptr<ServerWindow> window0(
      CreateTestWindow(&delegate, WindowId(0, 2), parent.get()));
  std::unique_ptr<ServerWindow> window1(
      CreateTestWindow(&delegate, WindowId(0, 3), parent.get()));

  ServerWindow* window2 =
      CreateTestWindow(&delegate, WindowId(0, 4), parent.get());
  window0->AddTransientWindow(window2);
  EXPECT_EQ("2 4 3", ChildWindowIDsAsString(parent.get()));
}

// Tests that windows are restacked properly after a call to
// AddTransientWindow() or RemoveTransientWindow().
TEST_F(TransientWindowsTest, RestackUponAddOrRemoveTransientWindow) {
  TestServerWindowDelegate delegate;
  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(0, 1), nullptr));
  std::unique_ptr<ServerWindow> windows[4];
  for (int i = 0; i < 4; i++)
    windows[i].reset(
        CreateTestWindow(&delegate, WindowId(0, i + 2), parent.get()));

  EXPECT_EQ("2 3 4 5", ChildWindowIDsAsString(parent.get()));

  windows[0]->AddTransientWindow(windows[2].get());
  EXPECT_EQ("2 4 3 5", ChildWindowIDsAsString(parent.get()));

  windows[0]->AddTransientWindow(windows[3].get());
  EXPECT_EQ("2 4 5 3", ChildWindowIDsAsString(parent.get()));

  windows[0]->RemoveTransientWindow(windows[2].get());
  EXPECT_EQ("2 5 4 3", ChildWindowIDsAsString(parent.get()));

  windows[0]->RemoveTransientWindow(windows[3].get());
  EXPECT_EQ("2 5 4 3", ChildWindowIDsAsString(parent.get()));
}

// Verifies TransientWindowObserver is notified appropriately.
TEST_F(TransientWindowsTest, TransientWindowObserverNotified) {
  TestServerWindowDelegate delegate;
  std::unique_ptr<ServerWindow> parent(
      CreateTestWindow(&delegate, WindowId(0, 1), nullptr));
  std::unique_ptr<ServerWindow> w1(
      CreateTestWindow(&delegate, WindowId(0, 2), parent.get()));

  TestTransientWindowObserver test_observer;
  parent->AddObserver(&test_observer);

  parent->AddTransientWindow(w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(0, test_observer.remove_count());

  parent->RemoveTransientWindow(w1.get());
  EXPECT_EQ(1, test_observer.add_count());
  EXPECT_EQ(1, test_observer.remove_count());

  parent->RemoveObserver(&test_observer);
}

}  // namespace ws
}  // namespace ui
