// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_media_keys_listener_win.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"

namespace ui {

namespace {

class MockMediaKeysListenerDelegate : public MediaKeysListener::Delegate {
 public:
  MockMediaKeysListenerDelegate() = default;
  ~MockMediaKeysListenerDelegate() override = default;

  // MediaKeysListener::Delegate implementation.
  void OnMediaKeysAccelerator(const Accelerator& accelerator) override {
    received_events_.push_back(accelerator.ToKeyEvent());

    // If we've received the events we're waiting for, stop waiting.
    if (key_event_wait_loop_ &&
        received_events_.size() >= num_key_events_to_wait_for_) {
      key_event_wait_loop_->Quit();
    }
  }

  void OnStartedWatchingMediaKeys() override {
    std::move(hook_registration_callback_).Run();
  }

  // Set the callback to be called when the keyboard hook has been registered.
  void SetHookRegistrationCallback(base::OnceClosure callback) {
    hook_registration_callback_ = std::move(callback);
  }

  // Loop until we've received |num_events| key events from the listener.
  void WaitForKeyEvents(uint32_t num_events) {
    key_event_wait_loop_ = std::make_unique<base::RunLoop>();
    if (received_events_.size() >= num_events)
      return;

    num_key_events_to_wait_for_ = num_events;
    key_event_wait_loop_->Run();
  }

  // Expect that we have received the correct number of key events.
  void ExpectReceivedEventsCount(uint32_t count) {
    EXPECT_EQ(count, received_events_.size());
  }

  // Expect that the key event received at |index| has the specified key code
  // and type.
  void ExpectReceivedEvent(uint32_t index, KeyboardCode code, EventType type) {
    ASSERT_LT(index, received_events_.size());
    KeyEvent* key_event = &received_events_.at(index);
    EXPECT_EQ(code, key_event->key_code());
    EXPECT_EQ(type, key_event->type());
  }

 private:
  std::vector<KeyEvent> received_events_;
  std::unique_ptr<base::RunLoop> key_event_wait_loop_;
  uint32_t num_key_events_to_wait_for_ = 0;
  base::OnceClosure hook_registration_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaKeysListenerDelegate);
};

}  // anonymous namespace

class GlobalMediaKeysListenerWinInteractiveTest : public testing::Test {
 public:
  GlobalMediaKeysListenerWinInteractiveTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  void SendKeyDown(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
  }

  void SendKeyUp(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
  }

  void StartListeningAndWaitForHookRegistration(
      GlobalMediaKeysListenerWin& listener,
      MockMediaKeysListenerDelegate& delegate,
      ui::KeyboardCode key_code) {
    base::RunLoop hook_registration_loop;
    delegate.SetHookRegistrationCallback(hook_registration_loop.QuitClosure());
    listener.StartWatchingMediaKey(key_code);
    hook_registration_loop.Run();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DWORD time_stamp_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaKeysListenerWinInteractiveTest);
};

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, SimplePlayPauseTest) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  StartListeningAndWaitForHookRegistration(listener, delegate,
                                           ui::VKEY_MEDIA_PLAY_PAUSE);

  // Send a key down event and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE,
                               ET_KEY_PRESSED);

  // Send a key up event and validate that it was received by the delegate.
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(2);
  delegate.ExpectReceivedEvent(/*index=*/1, ui::VKEY_MEDIA_PLAY_PAUSE,
                               ET_KEY_RELEASED);
}

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, HookCanBeRestarted) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  // Start listening to create the hook.
  StartListeningAndWaitForHookRegistration(listener, delegate,
                                           ui::VKEY_MEDIA_NEXT_TRACK);

  // Stop listening to delete the hook.
  listener.StopWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Start listening to recreate the hook.
  StartListeningAndWaitForHookRegistration(listener, delegate,
                                           ui::VKEY_MEDIA_PREV_TRACK);

  // Send a key down event and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_PREV_TRACK);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PREV_TRACK,
                               ET_KEY_PRESSED);

  // Send a key up event and validate that it was received by the delegate.
  SendKeyUp(ui::VKEY_MEDIA_PREV_TRACK);
  delegate.WaitForKeyEvents(2);
  delegate.ExpectReceivedEvent(/*index=*/1, ui::VKEY_MEDIA_PREV_TRACK,
                               ET_KEY_RELEASED);
}

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, ListenForMultipleKeys) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  StartListeningAndWaitForHookRegistration(listener, delegate,
                                           ui::VKEY_MEDIA_PLAY_PAUSE);
  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_STOP);

  // Send a key down event and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE,
                               ET_KEY_PRESSED);

  // Send a key up event and validate that it was received by the delegate.
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(2);
  delegate.ExpectReceivedEvent(/*index=*/1, ui::VKEY_MEDIA_PLAY_PAUSE,
                               ET_KEY_RELEASED);

  // Send a key down event and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_STOP);
  delegate.WaitForKeyEvents(3);
  delegate.ExpectReceivedEvent(/*index=*/2, ui::VKEY_MEDIA_STOP,
                               ET_KEY_PRESSED);

  // Send a key up event and validate that it was received by the delegate.
  SendKeyUp(ui::VKEY_MEDIA_STOP);
  delegate.WaitForKeyEvents(4);
  delegate.ExpectReceivedEvent(/*index=*/3, ui::VKEY_MEDIA_STOP,
                               ET_KEY_RELEASED);
}

}  // namespace ui