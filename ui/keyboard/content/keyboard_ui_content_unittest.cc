// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/content/keyboard_ui_content.h"

#include "content/public/browser/web_contents.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {
namespace {

class TestKeyboardUIContent : public KeyboardUIContent {
 public:
  TestKeyboardUIContent(content::BrowserContext* context,
                        content::WebContents* contents)
      : KeyboardUIContent(context), contents_(contents) {}
  ~TestKeyboardUIContent() override {}

  ui::InputMethod* GetInputMethod() override { return nullptr; }
  void SetUpdateInputType(ui::TextInputType type) override {}
  void RequestAudioInput(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {}

  content::WebContents* CreateWebContents() override { return contents_; }

 private:
  content::WebContents* contents_;
};

}  // namespace

class KeyboardUIContentTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    initializer_ = base::MakeUnique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    content::RenderViewHostTestHarness::TearDown();
    initializer_.reset();
  }

 private:
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
};

// A test for crbug.com/734534
TEST_F(KeyboardUIContentTest, DoesNotCrashWhenParentDoesNotExist) {
  content::WebContents* contents = CreateTestWebContents();
  TestKeyboardUIContent keyboard_ui(contents->GetBrowserContext(), contents);

  EXPECT_FALSE(keyboard_ui.HasKeyboardWindow());
  aura::Window* view = keyboard_ui.GetKeyboardWindow();
  EXPECT_TRUE(keyboard_ui.HasKeyboardWindow());

  EXPECT_FALSE(view->parent());

  // Change window size to trigger OnWindowBoundsChanged.
  view->SetBounds(gfx::Rect(0, 0, 1200, 800));
}

}  // namespace keyboard
