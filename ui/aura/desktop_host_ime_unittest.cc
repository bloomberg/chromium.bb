// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop_host.h"

#if defined(USE_X11)
#include <X11/keysym.h>
#include <X11/Xlib.h>
#undef Bool
#undef None
#undef Status
#endif

#include <cstring>

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_client.h"
#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif
#include "ui/gfx/rect.h"

#if !defined(USE_X11)
#define PostKeyEvent DISABLED_PostKeyEvent
#define PostKeyEventNoDummyDelegate DISABLED_PostKeyEventNoDummyDelegate
#define PostKeyEventWithInputMethodNoDummyDelegate \
DISABLED_PostKeyEventWithInputMethodNoDummyDelegate
#endif

namespace aura {
namespace test {

namespace {

// A dummy WindowDelegate implementation which quits a current message loop
// when it receives a key event.
class DummyWindowDelegate : public TestWindowDelegate {
 public:
  virtual ~DummyWindowDelegate() {
  }
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE {
    MessageLoopForUI::current()->Quit();
    return true;
  }
};

// A dummy InputMethodDelegate implementation which quits a current message loop
// when it receives a native or fabricated key event.
class DummyInputMethodDelegate : public ui::internal::InputMethodDelegate {
 public:
  DummyInputMethodDelegate() {
    ResetFlags();
  }
  virtual ~DummyInputMethodDelegate() {}

  virtual void DispatchKeyEventPostIME(
      const base::NativeEvent& native_key_event) OVERRIDE {
    has_key_event_ = true;
    MessageLoopForUI::current()->Quit();
  }
  virtual void DispatchFabricatedKeyEventPostIME(
      ui::EventType type, ui::KeyboardCode key_code, int flags) OVERRIDE {
    has_fabricated_key_event_ = true;
    MessageLoopForUI::current()->Quit();
  }

  void ResetFlags() {
    has_key_event_ = has_fabricated_key_event_ = false;
  }

  bool has_key_event_;
  bool has_fabricated_key_event_;
};

// A dummy TextInputClient implementation which remembers if InsertChar function
// is called.
class DummyTextInputClient : public ui::TextInputClient {
 public:
  DummyTextInputClient() {
    ResetFlag();
  }
  virtual ~DummyTextInputClient() {
  }

  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE {}
  virtual void ConfirmCompositionText() OVERRIDE {}
  virtual void ClearCompositionText() OVERRIDE {}
  virtual void InsertText(const string16& text) OVERRIDE {}
  virtual void InsertChar(char16 ch, int flags) OVERRIDE {
    has_char_event_ = true;
  }
  virtual ui::TextInputType GetTextInputType() const OVERRIDE {
    return ui::TEXT_INPUT_TYPE_NONE;
  }
  virtual gfx::Rect GetCaretBounds() OVERRIDE {
    return gfx::Rect();
  }
  virtual bool HasCompositionText() OVERRIDE {
    return false;
  }
  virtual bool GetTextRange(ui::Range* range) OVERRIDE {
    return false;
  }
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE {
    return false;
  }
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE {
    return false;
  }
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE {
    return false;
  }
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE {
    return false;
  }
  virtual bool GetTextFromRange(
      const ui::Range& range, string16* text) OVERRIDE {
    return false;
  }
  virtual void OnInputMethodChanged() OVERRIDE {
  }
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE {
    return false;
  }

  void ResetFlag() {
    has_char_event_ = false;
  }

  bool has_char_event_;
};

// Returns a native key press or key release event.
// TODO(yusukes): Add function parameters like |key_name| and |modifiers| so
// that it could generate an event other than XK_space.
base::NativeEvent SynthesizeKeyEvent(bool is_press) {
  base::NativeEvent event = base::NativeEvent();

#if defined(USE_X11)
  event = new XEvent;
  std::memset(event, 0, sizeof(XEvent));

  Display* display = ui::GetXDisplay();
  ::Window focused;
  int dummy;
  XGetInputFocus(display, &focused, &dummy);

  XKeyEvent* key_event = &event->xkey;
  key_event->display = display;
  key_event->keycode = XKeysymToKeycode(display, XK_space);
  key_event->root = ui::GetX11RootWindow();
  key_event->same_screen = True;
  key_event->send_event = False;
  key_event->state = 0;
  key_event->subwindow = 0L;  // None;
  key_event->time = CurrentTime;
  key_event->type = is_press ? KeyPress : KeyRelease;
  key_event->window = focused;
  key_event->x = key_event->x_root = key_event->y = key_event->y_root = 1;
#else
  // TODO(yusukes): Support Windows.
  NOTIMPLEMENTED();
#endif

  return event;
}

// Deletes a native event generated by SynthesizeKeyEvent() above.
void Delete(base::NativeEvent event) {
#if defined(USE_X11)
  delete event;
#else
  // TODO(yusukes): Support Windows.
  NOTIMPLEMENTED();
#endif
}

// Enters a nested message loop.
void RunMessageLoop() {
  MessageLoop* loop = MessageLoop::current();
  const bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
  aura::Desktop::GetInstance()->Run();
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

}  // namespace

class DesktopHostIMETest : public AuraTestBase {
 protected:
  DesktopHostIMETest() : desktop_(Desktop::GetInstance()),
                         host_(desktop_->host_.get()),
                         input_method_(new ui::MockInputMethod(host_)),
                         window_(CreateTestWindowWithDelegate(
                             &window_delegate_, -1,
                             gfx::Rect(2, 3, 4, 5), NULL)) {
  }

  virtual void SetUp() OVERRIDE {
    input_method_->Init(true);
    input_method_->SetFocusedTextInputClient(&text_input_client_);
    host_->SetInputMethod(input_method_);  // pass ownership
    AuraTestBase::SetUp();
  }

  Desktop* desktop_;
  DesktopHost* host_;
  ui::MockInputMethod* input_method_;

  DummyInputMethodDelegate input_method_delegate_;
  DummyTextInputClient text_input_client_;

  DummyWindowDelegate window_delegate_;
  scoped_ptr<aura::Window> window_;
};

// Test if DesktopHost correctly passes a key press/release event to an IME.
TEST_F(DesktopHostIMETest, PostKeyEvent) {
  input_method_->SetDelegate(&input_method_delegate_);

  // Press space. The press event should directly go to |input_method_delegate_|
  // and then, a 'Char' event should also be generated and passed to
  // |text_input_client_|.
  base::NativeEvent event = SynthesizeKeyEvent(true /* press */);
  host_->PostNativeEvent(event);
  // Enter a new message loop and wait for an InputMethodDelegate function to
  // be called.
  RunMessageLoop();
  EXPECT_TRUE(input_method_delegate_.has_key_event_);
  EXPECT_FALSE(input_method_delegate_.has_fabricated_key_event_);
  EXPECT_TRUE(text_input_client_.has_char_event_);
  input_method_delegate_.ResetFlags();
  text_input_client_.ResetFlag();
  Delete(event);

  // Release space. A Char event should NOT be generated.
  event = SynthesizeKeyEvent(false);
  host_->PostNativeEvent(event);
  RunMessageLoop();
  EXPECT_TRUE(input_method_delegate_.has_key_event_);
  EXPECT_FALSE(input_method_delegate_.has_fabricated_key_event_);
  EXPECT_FALSE(text_input_client_.has_char_event_);
  input_method_delegate_.ResetFlags();
  text_input_client_.ResetFlag();
  Delete(event);
}

// Do the same as above, but use the |host_| itself as ui::InputMethodDelegate
// to see if DesktopHost::DispatchKeyEventPostIME() function correctly passes
// a key event to the |desktop_|.
TEST_F(DesktopHostIMETest, PostKeyEventNoDummyDelegate) {
  input_method_->SetDelegate(host_);
  // Call SetActiveWindow here so DummyWindowDelegate could receive a key event.
  desktop_->SetActiveWindow(window_.get(), NULL);

  // Press space. DummyWindowDelegate will quit the loop. A Char event should
  // be generated.
  base::NativeEvent event = SynthesizeKeyEvent(true /* press */);
  host_->PostNativeEvent(event);
  RunMessageLoop();
  EXPECT_TRUE(text_input_client_.has_char_event_);
  text_input_client_.ResetFlag();
  Delete(event);

  // Release space.
  event = SynthesizeKeyEvent(false);
  host_->PostNativeEvent(event);
  RunMessageLoop();
  EXPECT_FALSE(text_input_client_.has_char_event_);
  text_input_client_.ResetFlag();
  Delete(event);
}

// Do the same as above, but this time, |input_method_| consumes the key press
// event. Checks if DesktopHost::DispatchFabricatedKeyEventPostIME() function
// called by |input_method_| correctly passes a VK_PROCESS key event to the
// |desktop_|.
TEST_F(DesktopHostIMETest, PostKeyEventWithInputMethodNoDummyDelegate) {
  input_method_->SetDelegate(host_);
  input_method_->ConsumeNextKey();
  desktop_->SetActiveWindow(window_.get(), NULL);

  // Press space. The press event will be consumed by the mock IME and will not
  // be passed to the delegate. Instead, a fabricated key event (VK_PROCESSKEY)
  // will be passed to it. Char event should not be generated this time.
  base::NativeEvent event = SynthesizeKeyEvent(true);
  host_->PostNativeEvent(event);
  RunMessageLoop();
  EXPECT_FALSE(text_input_client_.has_char_event_);
  text_input_client_.ResetFlag();
  Delete(event);

  // Release space. A Char event should not be generated.
  event = SynthesizeKeyEvent(false);
  host_->PostNativeEvent(event);
  RunMessageLoop();
  EXPECT_FALSE(text_input_client_.has_char_event_);
  text_input_client_.ResetFlag();
  Delete(event);
}

}  // namespace test
}  // namespace aura
