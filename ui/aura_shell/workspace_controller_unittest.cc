// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace_controller.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_desktop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"

namespace aura_shell {
namespace internal {

using aura::Window;

class WorkspaceControllerTest : public aura::test::AuraTestBase {
 public:
  WorkspaceControllerTest() {}
  virtual ~WorkspaceControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    contents_view_ = GetTestDesktopDelegate()->default_container();
    controller_.reset(new WorkspaceController(contents_view_));
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    contents_view()->AddChild(window);
    return window;
  }

  aura::Window * contents_view() {
    return contents_view_;
  }

  WorkspaceManager* workspace_manager() {
    return controller_->workspace_manager();
  }

  scoped_ptr<WorkspaceController> controller_;

 private:
  aura::Window* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceControllerTest);
};

TEST_F(WorkspaceControllerTest, Overview) {
  workspace_manager()->SetWorkspaceSize(gfx::Size(500, 300));

  // Creating two workspaces, ws1 which contains window w1,
  // and ws2 which contains window w2.
  Workspace* ws1 = workspace_manager()->CreateWorkspace();
  scoped_ptr<Window> w1(CreateTestWindow());

  EXPECT_TRUE(ws1->AddWindowAfter(w1.get(), NULL));

  Workspace* ws2 = workspace_manager()->CreateWorkspace();
  scoped_ptr<Window> w2(CreateTestWindow());
  EXPECT_TRUE(ws2->AddWindowAfter(w2.get(), NULL));

  // Activating a window switches the active workspace.
  w2->Activate();
  EXPECT_EQ(ws2, workspace_manager()->GetActiveWorkspace());

  // The size of contents_view() is now ws1(500) + ws2(500) + margin(50).
  EXPECT_EQ("0,0 1050x300", contents_view()->bounds().ToString());
  EXPECT_FALSE(workspace_manager()->is_overview());
  workspace_manager()->SetOverview(true);
  EXPECT_TRUE(workspace_manager()->is_overview());

  // Switching overview mode doesn't change the active workspace.
  EXPECT_EQ(ws2, workspace_manager()->GetActiveWorkspace());

  // Activaing window w1 switches the active window and
  // the mode back to normal mode.
  w1->Activate();
  EXPECT_EQ(ws1, workspace_manager()->GetActiveWorkspace());
  EXPECT_FALSE(workspace_manager()->is_overview());

  // Deleting w1 without DesktopDelegate resets the active workspace
  ws1->RemoveWindow(w1.get());
  delete ws1;
  w1.reset();
  EXPECT_EQ(NULL, workspace_manager()->GetActiveWorkspace());
  EXPECT_EQ("0,0 500x300", contents_view()->bounds().ToString());
  ws2->RemoveWindow(w2.get());
  delete ws2;
  // The size of contents_view() for no workspace case must be
  // same as one contents_view() case.
  EXPECT_EQ("0,0 500x300", contents_view()->bounds().ToString());

  // Reset now before windows are destroyed.
  controller_.reset();
}

}  // namespace internal
}  // namespace aura_shell
