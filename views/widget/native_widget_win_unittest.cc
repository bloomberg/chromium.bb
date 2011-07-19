// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_win.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace views {
namespace {

class NativeWidgetWinTest : public testing::Test {
 public:
  NativeWidgetWinTest() {
    OleInitialize(NULL);
  }

  ~NativeWidgetWinTest() {
    OleUninitialize();
  }

  virtual void TearDown() {
    // Flush the message loop because we have pending release tasks
    // and these tasks if un-executed would upset Valgrind.
    RunPendingMessages();
  }

  // Create a simple widget win. The caller is responsible for taking ownership
  // of the returned value.
  NativeWidgetWin* CreateNativeWidgetWin();

  void RunPendingMessages() {
    message_loop_.RunAllPending();
  }

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWinTest);
};

NativeWidgetWin* NativeWidgetWinTest::CreateNativeWidgetWin() {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  return static_cast<NativeWidgetWin*>(widget.release()->native_widget());
}

TEST_F(NativeWidgetWinTest, ZoomWindow) {
  scoped_ptr<NativeWidgetWin> window(CreateNativeWidgetWin());
  window->ShowWindow(SW_HIDE);
  EXPECT_FALSE(window->IsActive());
  window->ShowWindow(SW_MAXIMIZE);
  EXPECT_TRUE(window->IsZoomed());
  window->CloseNow();
}

TEST_F(NativeWidgetWinTest, SetBoundsForZoomedWindow) {
  scoped_ptr<NativeWidgetWin> window(CreateNativeWidgetWin());
  window->ShowWindow(SW_MAXIMIZE);
  EXPECT_TRUE(window->IsZoomed());

  // Create another window, so that it will be active.
  scoped_ptr<NativeWidgetWin> window2(CreateNativeWidgetWin());
  window2->ShowWindow(SW_MAXIMIZE);
  EXPECT_TRUE(window2->IsActive());
  EXPECT_FALSE(window->IsActive());

  // Verify that setting the bounds of a zoomed window will unzoom it and not
  // cause it to be activated.
  window->SetBounds(gfx::Rect(50, 50, 650, 650));
  EXPECT_FALSE(window->IsZoomed());
  EXPECT_FALSE(window->IsActive());

  // Cleanup.
  window->CloseNow();
  window2->CloseNow();
}

TEST_F(NativeWidgetWinTest, EnsureRectIsVisibleInRect) {
  gfx::Rect parent_rect(0, 0, 500, 400);

  {
    // Child rect x < 0
    gfx::Rect child_rect(-50, 20, 100, 100);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(10, 20, 100, 100), child_rect);
  }

  {
    // Child rect y < 0
    gfx::Rect child_rect(20, -50, 100, 100);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 10, 100, 100), child_rect);
  }

  {
    // Child rect right > parent_rect.right
    gfx::Rect child_rect(450, 20, 100, 100);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(390, 20, 100, 100), child_rect);
  }

  {
    // Child rect bottom > parent_rect.bottom
    gfx::Rect child_rect(20, 350, 100, 100);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 290, 100, 100), child_rect);
  }

  {
    // Child rect width > parent_rect.width
    gfx::Rect child_rect(20, 20, 700, 100);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 480, 100), child_rect);
  }

  {
    // Child rect height > parent_rect.height
    gfx::Rect child_rect(20, 20, 100, 700);
    internal::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 100, 380), child_rect);
  }
}


}  // namespace
}  // namespace views
