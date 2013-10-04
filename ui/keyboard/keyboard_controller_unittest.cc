// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace keyboard {
namespace {

// An event handler that focuses a window when it is clicked/touched on. This is
// used to match the focus manger behaviour in ash and views.
class TestFocusController : public ui::EventHandler {
 public:
  explicit TestFocusController(aura::RootWindow* root)
      : root_(root) {
    root_->AddPreTargetHandler(this);
  }

  virtual ~TestFocusController() {
    root_->RemovePreTargetHandler(this);
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_TOUCH_PRESSED) {
      aura::client::GetFocusClient(target)->FocusWindow(target);
    }
  }

  aura::Window* root_;
  DISALLOW_COPY_AND_ASSIGN(TestFocusController);
};

class TestKeyboardControllerProxy : public KeyboardControllerProxy {
 public:
  TestKeyboardControllerProxy()
      : window_(new aura::Window(&delegate_)),
        input_method_(ui::CreateInputMethod(NULL,
                                            gfx::kNullAcceleratedWidget)) {
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
  }

  virtual ~TestKeyboardControllerProxy() {
    // Destroy the window before the delegate.
    window_.reset();
  }

  // Overridden from KeyboardControllerProxy:
  virtual aura::Window* GetKeyboardWindow() OVERRIDE { return window_.get(); }
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE { return NULL; }
  virtual ui::InputMethod* GetInputMethod() OVERRIDE {
    return input_method_.get();
  }
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE { return; }

 private:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerProxy);
};

// Keeps a count of all the events a window receives.
class EventObserver : public ui::EventHandler {
 public:
  EventObserver() {}
  virtual ~EventObserver() {}

  int GetEventCount(ui::EventType type) {
    return event_counts_[type];
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    ui::EventHandler::OnEvent(event);
    event_counts_[event->type()]++;
  }

  std::map<ui::EventType, int> event_counts_;
  DISALLOW_COPY_AND_ASSIGN(EventObserver);
};

class TestTextInputClient : public ui::TextInputClient {
 public:
  explicit TestTextInputClient(ui::TextInputType type)
      : type_(type) {}
  virtual ~TestTextInputClient() {}

 private:
  // Overridden from ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE {}
  virtual void ConfirmCompositionText() OVERRIDE {}
  virtual void ClearCompositionText() OVERRIDE {}
  virtual void InsertText(const base::string16& text) OVERRIDE {}
  virtual void InsertChar(base::char16 ch, int flags) OVERRIDE {}
  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE {
    return static_cast<gfx::NativeWindow>(NULL);
  }
  virtual ui::TextInputType GetTextInputType() const OVERRIDE {
    return type_;
  }
  virtual ui::TextInputMode GetTextInputMode() const OVERRIDE {
    return ui::TEXT_INPUT_MODE_DEFAULT;
  }
  virtual bool CanComposeInline() const OVERRIDE { return false; }
  virtual gfx::Rect GetCaretBounds() OVERRIDE { return gfx::Rect(); }

  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) OVERRIDE {
    return false;
  }
  virtual bool HasCompositionText() OVERRIDE { return false; }
  virtual bool GetTextRange(gfx::Range* range) OVERRIDE { return false; }
  virtual bool GetCompositionTextRange(gfx::Range* range) OVERRIDE {
    return false;
  }
  virtual bool GetSelectionRange(gfx::Range* range) OVERRIDE { return false; }
  virtual bool SetSelectionRange(const gfx::Range& range) OVERRIDE {
    return false;
  }
  virtual bool DeleteRange(const gfx::Range& range) OVERRIDE { return false; }
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) OVERRIDE {
    return false;
  }
  virtual void OnInputMethodChanged() OVERRIDE {}
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE { return false; }
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) OVERRIDE {}
  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE {}

  ui::TextInputType type_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class KeyboardContainerObserver : public aura::WindowObserver {
 public:
  explicit KeyboardContainerObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  virtual ~KeyboardContainerObserver() {
    window_->RemoveObserver(this);
  }

 private:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE {
    if (!visible)
      base::MessageLoop::current()->Quit();
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerObserver);
};

}  // namespace

class KeyboardControllerTest : public testing::Test {
 public:
  KeyboardControllerTest() {}
  virtual ~KeyboardControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();
    ui::SetUpInputMethodFactoryForTesting();
    focus_controller_.reset(new TestFocusController(root_window()));
    proxy_ = new TestKeyboardControllerProxy();
    controller_.reset(new KeyboardController(proxy_));
  }

  virtual void TearDown() OVERRIDE {
    focus_controller_.reset();
    aura_test_helper_->TearDown();
  }

  aura::RootWindow* root_window() { return aura_test_helper_->root_window(); }
  KeyboardControllerProxy* proxy() { return proxy_; }
  KeyboardController* controller() { return controller_.get(); }

  void ShowKeyboard() {
    TestTextInputClient test_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
    controller_->OnTextInputStateChanged(&test_text_input_client);
  }

 protected:
  bool WillHideKeyboard() {
    return controller_->WillHideKeyboard();
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  scoped_ptr<TestFocusController> focus_controller_;

 private:
  KeyboardControllerProxy* proxy_;
  scoped_ptr<KeyboardController> controller_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerTest);
};

TEST_F(KeyboardControllerTest, KeyboardSize) {
  aura::Window* container(controller()->GetContainerWindow());
  gfx::Rect bounds(0, 0, 100, 100);
  container->SetBounds(bounds);

  const gfx::Rect& before_bounds = proxy()->GetKeyboardWindow()->bounds();
  gfx::Rect new_bounds(
      before_bounds.x(), before_bounds.y(),
      before_bounds.width() / 2, before_bounds.height() / 2);

  // The KeyboardController's LayoutManager shouldn't let this happen
  proxy()->GetKeyboardWindow()->SetBounds(new_bounds);
  ASSERT_EQ(before_bounds, proxy()->GetKeyboardWindow()->bounds());
}

// Tests that tapping/clicking inside the keyboard does not give it focus.
TEST_F(KeyboardControllerTest, ClickDoesNotFocusKeyboard) {
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);

  root_window()->AddChild(keyboard_container);
  keyboard_container->Show();

  ShowKeyboard();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());

  // Click on the keyboard. Make sure the keyboard receives the event, but does
  // not get focus.
  EventObserver observer;
  keyboard_container->AddPreTargetHandler(&observer);

  aura::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(proxy()->GetKeyboardWindow()->bounds().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_RELEASED));

  // Click outside of the keyboard. It should reach the window behind.
  generator.MoveMouseTo(gfx::Point());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
}

TEST_F(KeyboardControllerTest, VisibilityChangeWithTextInputTypeChange) {
  const gfx::Rect& root_bounds = root_window()->bounds();

  ui::InputMethod* input_method = proxy()->GetInputMethod();
  TestTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  TestTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  TestTextInputClient input_client_2(ui::TEXT_INPUT_TYPE_TEXT);
  TestTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  TestTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);
  input_method->SetFocusedTextInputClient(&input_client_0);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  scoped_ptr<KeyboardContainerObserver> keyboard_container_observer(
      new KeyboardContainerObserver(keyboard_container));
  keyboard_container->SetBounds(root_bounds);
  root_window()->AddChild(keyboard_container);

  EXPECT_TRUE(keyboard_container->IsVisible());

  input_method->SetFocusedTextInputClient(&no_input_client_0);
  // Keyboard should not immediately hide itself. It is delayed to avoid layout
  // flicker when the focus of input field quickly change.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(WillHideKeyboard());
  // Wait for hide keyboard to finish.
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(keyboard_container->IsVisible());

  input_method->SetFocusedTextInputClient(&input_client_1);
  EXPECT_TRUE(keyboard_container->IsVisible());

  // Schedule to hide keyboard.
  input_method->SetFocusedTextInputClient(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());
  // Cancel keyboard hide.
  input_method->SetFocusedTextInputClient(&input_client_2);

  EXPECT_FALSE(WillHideKeyboard());
  EXPECT_TRUE(keyboard_container->IsVisible());
}

}  // namespace keyboard
