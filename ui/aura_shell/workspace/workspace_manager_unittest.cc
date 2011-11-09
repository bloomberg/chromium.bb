// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace_manager.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_observer.h"
#include "ui/base/ui_base_types.h"

namespace {
using aura_shell::internal::Workspace;
using aura_shell::internal::WorkspaceManager;
using aura::Window;

class TestWorkspaceObserver : public aura_shell::internal::WorkspaceObserver {
 public:
  explicit TestWorkspaceObserver(WorkspaceManager* manager)
      : manager_(manager),
        move_source_(NULL),
        move_target_(NULL),
        active_workspace_(NULL),
        old_active_workspace_(NULL) {
    manager_->AddObserver(this);
  }

  virtual ~TestWorkspaceObserver() {
    manager_->RemoveObserver(this);
  }

  Window* move_source() { return move_source_; }
  Window* move_target() { return move_target_; }
  Workspace* active_workspace() { return active_workspace_; }
  Workspace* old_active_workspace() { return old_active_workspace_; }

  // Resets the observer states.
  void reset() {
    active_workspace_ = NULL;
    old_active_workspace_ = NULL;
    move_source_ = NULL;
    move_target_ = NULL;
  }

  // Overridden from WorkspaceObserver:
  virtual void WindowMoved(WorkspaceManager* manager,
                           Window* source,
                           Window* target) {
    move_source_ = source;
    move_target_ = target;
  }
  virtual void ActiveWorkspaceChanged(WorkspaceManager* manager,
                                      Workspace* old) {
    old_active_workspace_ = old;
    active_workspace_ = manager->GetActiveWorkspace();
  }

 private:
  WorkspaceManager* manager_;
  Window* move_source_;
  Window* move_target_;
  Workspace* active_workspace_;
  Workspace* old_active_workspace_;

  DISALLOW_COPY_AND_ASSIGN(TestWorkspaceObserver);
};

}  // namespace

namespace aura_shell {
namespace internal {

class WorkspaceManagerTestBase : public aura::test::AuraTestBase {
 public:
  WorkspaceManagerTestBase() {}
  virtual ~WorkspaceManagerTestBase() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    manager_.reset(new WorkspaceManager(viewport()));
  }

  virtual void TearDown() OVERRIDE {
    manager_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    return window;
  }

  aura::Window* viewport() {
    return GetTestStackingClient()->default_container();
  }
  scoped_ptr<WorkspaceManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceManagerTestBase);
};

class WorkspaceManagerTest : public WorkspaceManagerTestBase {
};

TEST_F(WorkspaceManagerTest, WorkspaceManagerCreateAddFind) {
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());

  Workspace* ws1 = manager_->CreateWorkspace();
  ws1->AddWindowAfter(w1.get(), NULL);
  // w2 is not a part of any workspace yet.
  EXPECT_EQ(NULL, manager_->FindBy(w2.get()));

  // w2 is in ws2 workspace.
  Workspace* ws2 = manager_->CreateWorkspace();
  ws2->AddWindowAfter(w2.get(), NULL);
  EXPECT_EQ(ws2, manager_->FindBy(w2.get()));

  // Make sure |FindBy(w1.get())| still returns
  // correct workspace.
  EXPECT_EQ(ws1, manager_->FindBy(w1.get()));

  // once workspace is gone, w2 shouldn't match
  // any workspace.
  delete ws2;
  EXPECT_EQ(NULL, manager_->FindBy(w2.get()));

  // Reset now before windows are destroyed.
  manager_.reset();
}

TEST_F(WorkspaceManagerTest, LayoutWorkspaces) {
  manager_->SetWorkspaceSize(gfx::Size(100, 100));
  EXPECT_EQ("0,0 100x100", viewport()->bounds().ToString());

  Workspace* ws1 = manager_->CreateWorkspace();
  manager_->LayoutWorkspaces();

  // ws1 is laied out in left most position.
  EXPECT_EQ(100, viewport()->bounds().width());
  EXPECT_EQ("0,0 100x100", ws1->bounds().ToString());

  // ws2 is laied out next to ws1, with 50 margin.
  Workspace* ws2 = manager_->CreateWorkspace();
  manager_->LayoutWorkspaces();

  EXPECT_EQ(250, viewport()->bounds().width());
  EXPECT_EQ("0,0 100x100", ws1->bounds().ToString());
  EXPECT_EQ("150,0 100x100", ws2->bounds().ToString());
}

// Makes sure the bounds of window are resized if the workspace size shrinks.
TEST_F(WorkspaceManagerTest, ResizeDuringLayout) {
  manager_->SetWorkspaceSize(gfx::Size(100, 100));
  EXPECT_EQ("0,0 100x100", viewport()->bounds().ToString());

  Workspace* ws1 = manager_->CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  viewport()->AddChild(w1.get());
  EXPECT_TRUE(ws1->AddWindowAfter(w1.get(), NULL));
  manager_->SetWorkspaceSize(gfx::Size(50, 50));

  // ws1 is laied out in left most position.
  EXPECT_EQ("0,0 50x50", ws1->bounds().ToString());
  EXPECT_EQ("0,0 50x50", w1->layer()->GetTargetBounds().ToString());
}

TEST_F(WorkspaceManagerTest, WorkspaceManagerDragArea) {
  aura::Desktop::GetInstance()->screen()->set_work_area_insets(
      gfx::Insets(10, 10, 10, 10));
  viewport()->SetBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ("10,10 180x180", manager_->GetDragAreaBounds().ToString());
}

TEST_F(WorkspaceManagerTest, WorkspaceManagerActivate) {
  TestWorkspaceObserver observer(manager_.get());
  Workspace* ws1 = manager_->CreateWorkspace();
  Workspace* ws2 = manager_->CreateWorkspace();
  EXPECT_EQ(NULL, manager_->GetActiveWorkspace());

  // Activate ws1.
  ws1->Activate();
  EXPECT_EQ(ws1, manager_->GetActiveWorkspace());
  EXPECT_EQ(NULL, observer.old_active_workspace());
  EXPECT_EQ(ws1, observer.active_workspace());
  observer.reset();

  // Activate ws2.
  ws2->Activate();
  EXPECT_EQ(ws2, manager_->GetActiveWorkspace());
  EXPECT_EQ(ws1, observer.old_active_workspace());
  EXPECT_EQ(ws2, observer.active_workspace());
  observer.reset();

  // Deleting active workspace sets active workspace to NULL.
  delete ws2;
  EXPECT_EQ(NULL, manager_->GetActiveWorkspace());
  EXPECT_EQ(ws2, observer.old_active_workspace());
  EXPECT_EQ(NULL, observer.active_workspace());
  manager_.reset();
}

TEST_F(WorkspaceManagerTest, FindRotateWindow) {
  manager_->SetWorkspaceSize(gfx::Size(500, 300));

  Workspace* ws1 = manager_->CreateWorkspace();
  scoped_ptr<Window> w11(CreateTestWindow());
  w11->SetBounds(gfx::Rect(0, 0, 100, 100));
  ws1->AddWindowAfter(w11.get(), NULL);

  scoped_ptr<Window> w12(CreateTestWindow());
  w12->SetBounds(gfx::Rect(0, 0, 100, 100));
  ws1->AddWindowAfter(w12.get(), NULL);
  manager_->LayoutWorkspaces();

  // Workspaces are 0-<lmgn>-145-<w11>-245-<wmng>-255-<w12>-355-<rmgn>-500.
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(0, 0)));
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(100, 0)));
  EXPECT_EQ(w11.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(150, 0)));
  EXPECT_EQ(w12.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(300, 0)));
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(400, 0)));

  w11->SetBounds(gfx::Rect(0, 0, 400, 100));
  w12->SetBounds(gfx::Rect(0, 0, 200, 100));
  manager_->FindBy(w11.get())->Layout(NULL);
  EXPECT_EQ(w11.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(10, 0)));
  EXPECT_EQ(w11.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(240, 0)));
  EXPECT_EQ(w12.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(260, 0)));
  EXPECT_EQ(w12.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(490, 0)));

  Workspace* ws2 = manager_->CreateWorkspace();
  scoped_ptr<Window> w21(CreateTestWindow());
  w21->SetBounds(gfx::Rect(0, 0, 100, 100));
  ws2->AddWindowAfter(w21.get(), NULL);
  manager_->LayoutWorkspaces();

  // 2nd workspace starts from 500+50 and the window is centered 750-850.
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(600, 0)));
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(740, 0)));
  EXPECT_EQ(w21.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(760, 0)));
  EXPECT_EQ(w21.get(),
            manager_->FindRotateWindowForLocation(gfx::Point(840, 0)));
  EXPECT_EQ(NULL, manager_->FindRotateWindowForLocation(gfx::Point(860, 0)));

  // Reset now before windows are destroyed.
  manager_.reset();
}

TEST_F(WorkspaceManagerTest, RotateWindows) {
  TestWorkspaceObserver observer(manager_.get());
  Workspace* ws1 = manager_->CreateWorkspace();
  Workspace* ws2 = manager_->CreateWorkspace();

  scoped_ptr<Window> w11(CreateTestWindow());
  ws1->AddWindowAfter(w11.get(), NULL);

  scoped_ptr<Window> w21(CreateTestWindow());
  scoped_ptr<Window> w22(CreateTestWindow());
  ws2->AddWindowAfter(w21.get(), NULL);
  ws2->AddWindowAfter(w22.get(), NULL);

  EXPECT_EQ(0, ws1->GetIndexOf(w11.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w21.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w22.get()));

  // Rotate right most to left most.
  manager_->RotateWindows(w22.get(), w11.get());
  EXPECT_EQ(w22.get(), observer.move_source());
  EXPECT_EQ(w11.get(), observer.move_target());

  EXPECT_EQ(0, ws1->GetIndexOf(w22.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w11.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w21.get()));

  // Rotate left most to right most.
  manager_->RotateWindows(w22.get(), w21.get());
  EXPECT_EQ(0, ws1->GetIndexOf(w11.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w21.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w22.get()));
  EXPECT_EQ(w22.get(), observer.move_source());
  EXPECT_EQ(w21.get(), observer.move_target());

  // Rotate left most to 1st element in 2nd workspace.
  manager_->RotateWindows(w11.get(), w21.get());
  EXPECT_EQ(0, ws1->GetIndexOf(w21.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w11.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w22.get()));
  EXPECT_EQ(w11.get(), observer.move_source());
  EXPECT_EQ(w21.get(), observer.move_target());

  // Rotate middle to right most.
  manager_->RotateWindows(w11.get(), w22.get());
  EXPECT_EQ(0, ws1->GetIndexOf(w21.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w22.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w11.get()));
  EXPECT_EQ(w11.get(), observer.move_source());
  EXPECT_EQ(w22.get(), observer.move_target());

  // Rotate middle to left most.
  manager_->RotateWindows(w22.get(), w21.get());
  EXPECT_EQ(0, ws1->GetIndexOf(w22.get()));
  EXPECT_EQ(0, ws2->GetIndexOf(w21.get()));
  EXPECT_EQ(1, ws2->GetIndexOf(w11.get()));
  EXPECT_EQ(w22.get(), observer.move_source());
  EXPECT_EQ(w21.get(), observer.move_target());

  // Reset now before windows are destroyed.
  manager_.reset();
}

class WorkspaceTest : public WorkspaceManagerTestBase {
};

TEST_F(WorkspaceTest, WorkspaceBasic) {
  Workspace* ws = manager_->CreateWorkspace();
  // Sanity check
  EXPECT_TRUE(ws->is_empty());

  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  scoped_ptr<Window> w3(CreateTestWindow());
  // ws is empty and can accomodate new window.
  EXPECT_TRUE(ws->CanAdd(w1.get()));

  // Add w1.
  EXPECT_TRUE(ws->AddWindowAfter(w1.get(), NULL));
  EXPECT_TRUE(ws->Contains(w1.get()));
  EXPECT_FALSE(ws->is_empty());

  // The workspac still has room for next window.
  EXPECT_TRUE(ws->CanAdd(w2.get()));
  EXPECT_TRUE(ws->AddWindowAfter(w2.get(), NULL));
  EXPECT_TRUE(ws->Contains(w2.get()));

  // The workspace no longer accepts new window.
  EXPECT_FALSE(ws->CanAdd(w3.get()));
  EXPECT_FALSE(ws->AddWindowAfter(w3.get(), NULL));
  EXPECT_FALSE(ws->Contains(w3.get()));

  // Check if the window has correct layout index.
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(-1, ws->GetIndexOf(w3.get()));

  // w1 is gone, so no index for w2.
  ws->RemoveWindow(w1.get());
  EXPECT_EQ(-1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_FALSE(ws->Contains(w1.get()));

  // Add w1 back. w1 now has index = 1.
  EXPECT_TRUE(ws->AddWindowAfter(w1.get(), w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w1.get()));

  // Reset now before windows are destroyed.
  manager_.reset();
}

TEST_F(WorkspaceTest, RotateWindows) {
  size_t orig_max = Workspace::SetMaxWindowsCount(3);
  Workspace* ws = manager_->CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  scoped_ptr<Window> w3(CreateTestWindow());
  ws->AddWindowAfter(w1.get(), NULL);
  ws->AddWindowAfter(w2.get(), NULL);
  ws->AddWindowAfter(w3.get(), NULL);

  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(2, ws->GetIndexOf(w3.get()));

  // Rotate to left.
  ws->RotateWindows(w1.get(), w3.get());
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(2, ws->GetIndexOf(w1.get()));

  // Rotate to right.
  ws->RotateWindows(w1.get(), w2.get());
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(2, ws->GetIndexOf(w3.get()));

  // Rotating to the middle from left.
  ws->RotateWindows(w1.get(), w2.get());
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(2, ws->GetIndexOf(w3.get()));

  // Rotating to the middle from right.
  ws->RotateWindows(w3.get(), w1.get());
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(2, ws->GetIndexOf(w1.get()));

  // Reset now before windows are destroyed.
  manager_.reset();
  Workspace::SetMaxWindowsCount(orig_max);
}

TEST_F(WorkspaceTest, ShiftWindowsSingle) {
  Workspace* ws = manager_->CreateWorkspace();
  // Single window in a workspace case.
  scoped_ptr<Window> w1(CreateTestWindow());
  ws->AddWindowAfter(w1.get(), NULL);

  scoped_ptr<Window> w2(CreateTestWindow());

  // Sanity check.
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));

  // Insert |w2| at the beginning and shift.
  aura::Window* overflow =
      ws->ShiftWindows(
          w2.get(), w2.get(), NULL, Workspace::SHIFT_TO_RIGHT);
  EXPECT_EQ(w1.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));

  // Insert |w1| at the end and shift.
  overflow = ws->ShiftWindows(
      w1.get(), w1.get(), NULL, Workspace::SHIFT_TO_LEFT);
  EXPECT_EQ(w2.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));

  // Insert |w2| at the begining and shift up to the w1.
  overflow = ws->ShiftWindows(
      w2.get(), w1.get(), NULL, Workspace::SHIFT_TO_RIGHT);
  EXPECT_EQ(NULL, overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));

  // Insert |w1| at the end and shift up to the w2.
  overflow = ws->ShiftWindows(
      w1.get(), w2.get(), NULL, Workspace::SHIFT_TO_LEFT);
  EXPECT_EQ(NULL, overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));

  // Reset now before windows are destroyed.
  manager_.reset();
}

TEST_F(WorkspaceTest, ShiftWindowsMultiple) {
  Workspace* ws = manager_->CreateWorkspace();
  // Single window in a workspace case.
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  ws->AddWindowAfter(w1.get(), NULL);
  ws->AddWindowAfter(w2.get(), NULL);

  scoped_ptr<Window> w3(CreateTestWindow());

  // Sanity check.
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(-1, ws->GetIndexOf(w3.get()));

  // Insert |w3| at the beginning and shift.
  aura::Window* overflow =
      ws->ShiftWindows(w3.get(), w3.get(), NULL,
                       Workspace::SHIFT_TO_RIGHT);
  EXPECT_EQ(w2.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w1.get()));

  // Insert |w3| at the end and shift.
  overflow = ws->ShiftWindows(w2.get(), w2.get(), NULL,
                              Workspace::SHIFT_TO_LEFT);
  EXPECT_EQ(w3.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));

  // Insert |w3| at the begining and shift up to the w1.
  overflow = ws->ShiftWindows(w3.get(), w1.get(), NULL,
                              Workspace::SHIFT_TO_RIGHT);
  EXPECT_EQ(NULL, overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w2.get()));

  // Insert |w1| at the end and shift up to the w2.
  overflow = ws->ShiftWindows(w1.get(), w2.get(), NULL,
                              Workspace::SHIFT_TO_LEFT);
  EXPECT_EQ(NULL, overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w3.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w1.get()));

  scoped_ptr<Window> unused(CreateTestWindow());

  // Insert |w2| at the |w3| and shift to right.
  overflow = ws->ShiftWindows(w2.get(), unused.get(), w3.get(),
                              Workspace::SHIFT_TO_RIGHT);
  EXPECT_EQ(w1.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w3.get()));

  // Insert |w1| at the |w2| and shift to left.
  overflow = ws->ShiftWindows(w1.get(), unused.get(), w2.get(),
                              Workspace::SHIFT_TO_LEFT);
  EXPECT_EQ(w2.get(), overflow);
  EXPECT_EQ(-1, ws->GetIndexOf(w2.get()));
  EXPECT_EQ(0, ws->GetIndexOf(w1.get()));
  EXPECT_EQ(1, ws->GetIndexOf(w3.get()));

  // Reset now before windows are destroyed.
  manager_.reset();
}

TEST_F(WorkspaceTest, ContainsFullscreenWindow) {
  Workspace* ws = manager_->CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  ws->AddWindowAfter(w1.get(), NULL);
  ws->AddWindowAfter(w2.get(), NULL);
  w1->Show();
  w2->Show();

  EXPECT_FALSE(ws->ContainsFullscreenWindow());

  w1->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_TRUE(ws->ContainsFullscreenWindow());

  w1->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_FALSE(ws->ContainsFullscreenWindow());

  w2->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_TRUE(ws->ContainsFullscreenWindow());

  w2->Hide();
  EXPECT_FALSE(ws->ContainsFullscreenWindow());
}

}  // namespace internal
}  // namespace aura_shell
