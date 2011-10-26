// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/aura_shell/workspace/workspace.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_desktop_delegate.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"

namespace {

class WorkspaceManagerTestBase : public aura::test::AuraTestBase {
 public:
  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    return window;
  }

  aura::Window* viewport() {
    return GetTestDesktopDelegate()->default_container();
  }
};

}  // namespace

namespace aura_shell {

using aura::Window;
using aura_shell::Workspace;
using aura_shell::WorkspaceManager;

class WorkspaceManagerTest : public WorkspaceManagerTestBase {
};

TEST_F(WorkspaceManagerTest, WorkspaceManagerCreateAddFind) {
  WorkspaceManager manager(viewport());
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());

  Workspace* ws1 = manager.CreateWorkspace();
  ws1->AddWindowAfter(w1.get(), NULL);
  // w2 is not a part of any workspace yet.
  EXPECT_EQ(NULL, manager.FindBy(w2.get()));

  // w2 is in ws2 workspace.
  Workspace* ws2 = manager.CreateWorkspace();
  ws2->AddWindowAfter(w2.get(), NULL);
  EXPECT_EQ(ws2, manager.FindBy(w2.get()));

  // Make sure |FindBy(w1.get())| still returns
  // correct workspace.
  EXPECT_EQ(ws1, manager.FindBy(w1.get()));

  // once workspace is gone, w2 shouldn't match
  // any workspace.
  delete ws2;
  EXPECT_EQ(NULL, manager.FindBy(w2.get()));
}

TEST_F(WorkspaceManagerTest, LayoutWorkspaces) {
  WorkspaceManager manager(viewport());
  manager.workspace_size_ = gfx::Size(100, 100);
  manager.LayoutWorkspaces();
  EXPECT_EQ("0,0 100x100", viewport()->bounds().ToString());

  Workspace* ws1 = manager.CreateWorkspace();
  manager.LayoutWorkspaces();

  // ws1 is laied out in left most position.
  EXPECT_EQ(100, viewport()->bounds().width());
  EXPECT_EQ("0,0 100x100", ws1->bounds().ToString());

  // ws2 is laied out next to ws1, with 50 margin.
  Workspace* ws2 = manager.CreateWorkspace();
  manager.LayoutWorkspaces();

  EXPECT_EQ(250, viewport()->bounds().width());
  EXPECT_EQ("0,0 100x100", ws1->bounds().ToString());
  EXPECT_EQ("150,0 100x100", ws2->bounds().ToString());
}

TEST_F(WorkspaceManagerTest, WorkspaceManagerDragArea) {
  aura::Desktop::GetInstance()->screen()->set_work_area_insets(
      gfx::Insets(10, 10, 10, 10));

  WorkspaceManager manager(viewport());
  viewport()->SetBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ("10,10 180x180", manager.GetDragAreaBounds().ToString());
}

TEST_F(WorkspaceManagerTest, Overview) {
  WorkspaceManager manager(viewport());
  manager.workspace_size_ = gfx::Size(500, 300);

  // Creating two workspaces, ws1 which contains window w1,
  // and ws2 which contains window w2.
  Workspace* ws1 = manager.CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());
  viewport()->AddChild(w1.get());
  EXPECT_TRUE(ws1->AddWindowAfter(w1.get(), NULL));

  Workspace* ws2 = manager.CreateWorkspace();
  scoped_ptr<Window> w2(CreateTestWindow());
  viewport()->AddChild(w2.get());
  EXPECT_TRUE(ws2->AddWindowAfter(w2.get(), NULL));

  // Activating a window switches the active workspace.
  w2->Activate();
  EXPECT_EQ(ws2, manager.GetActiveWorkspace());

  // The size of viewport() is now ws1(500) + ws2(500) + margin(50).
  EXPECT_EQ("0,0 1050x300", viewport()->bounds().ToString());
  EXPECT_FALSE(manager.is_overview());
  manager.SetOverview(true);
  EXPECT_TRUE(manager.is_overview());

  // Switching overview mode doesn't change the active workspace.
  EXPECT_EQ(ws2, manager.GetActiveWorkspace());

  // Activaing window w1 switches the active window and
  // the mode back to normal mode.
  w1->Activate();
  EXPECT_EQ(ws1, manager.GetActiveWorkspace());
  EXPECT_FALSE(manager.is_overview());

  // Deleting w1 without DesktopDelegate resets the active workspace
  ws1->RemoveWindow(w1.get());
  delete ws1;
  w1.reset();
  EXPECT_EQ(NULL, manager.GetActiveWorkspace());

  EXPECT_EQ("0,0 500x300", viewport()->bounds().ToString());
  ws2->RemoveWindow(w2.get());
  delete ws2;
  // The size of viewport() for no workspace case must be
  // same as one viewport() case.
  EXPECT_EQ("0,0 500x300", viewport()->bounds().ToString());
}

TEST_F(WorkspaceManagerTest, WorkspaceManagerActivate) {
  WorkspaceManager manager(viewport());
  Workspace* ws1 = manager.CreateWorkspace();
  Workspace* ws2 = manager.CreateWorkspace();
  EXPECT_EQ(NULL, manager.GetActiveWorkspace());

  // Activate ws1.
  ws1->Activate();
  EXPECT_EQ(ws1, manager.GetActiveWorkspace());

  // Activate ws2.
  ws2->Activate();
  EXPECT_EQ(ws2, manager.GetActiveWorkspace());

  // Deleting active workspace sets active workspace to NULL.
  delete ws2;
  EXPECT_EQ(NULL, manager.GetActiveWorkspace());
}

class WorkspaceTest : public WorkspaceManagerTestBase {
};

TEST_F(WorkspaceTest, WorkspaceBasic) {
  WorkspaceManager manager(viewport());

  Workspace* ws = manager.CreateWorkspace();
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
}

}  // namespace aura_shell
