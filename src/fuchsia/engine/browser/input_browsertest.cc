// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/input/virtualkeyboard/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl_test_base.h>
#include <memory>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/scoped_connection_checker.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/features.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "fuchsia/engine/test/scenic_test_helper.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::input::Key;
using fuchsia::ui::input3::KeyEvent;
using fuchsia::ui::input3::KeyEventType;
using fuchsia::ui::input3::KeyMeaning;
using fuchsia::ui::input3::NonPrintableKey;

namespace {

const char kKeyDown[] = "keydown";
const char kKeyPress[] = "keypress";
const char kKeyUp[] = "keyup";
const char kKeyDicts[] = "keyDicts";

KeyEvent CreateKeyEvent(Key key, KeyEventType event_type) {
  KeyEvent key_event;
  key_event.set_timestamp(base::TimeTicks::Now().ToZxTime());
  key_event.set_type(event_type);
  key_event.set_key(key);
  return key_event;
}

KeyEvent CreateCharacterEvent(uint32_t codepoint, KeyEventType event_type) {
  KeyEvent key_event;

  fuchsia::ui::input3::KeyMeaning meaning;
  meaning.set_codepoint(codepoint);
  key_event.set_key_meaning(std::move(meaning));
  key_event.set_type(event_type);
  key_event.set_timestamp(base::TimeTicks::Now().ToZxTime());
  return key_event;
}

base::Value ExpectedKeyValue(base::StringPiece code,
                             base::StringPiece key,
                             base::StringPiece type) {
  base::Value expected(base::Value::Type::DICTIONARY);
  expected.SetStringKey("code", code);
  expected.SetStringKey("key", key);
  expected.SetStringKey("type", type);
  return expected;
}

// Recursive base case.
template <typename T>
void AppendValueList(std::vector<T>* vec) {}

// Use tail recursion to emplace a sequence of Values into |vec|.
// It is used as an alternative to initializer lists, which don't work with
// move-only types like base::Value.
template <typename T, typename... Args>
void AppendValueList(std::vector<T>* vec, T&& value, Args&&... args) {
  vec->push_back(std::move(value));
  AppendValueList(vec, std::forward<base::Value>(args)...);
}

class FakeKeyboard : public fuchsia::ui::input3::testing::Keyboard_TestBase {
 public:
  explicit FakeKeyboard(sys::OutgoingDirectory* additional_services)
      : binding_(additional_services, this) {}
  ~FakeKeyboard() override = default;

  FakeKeyboard(const FakeKeyboard&) = delete;
  FakeKeyboard& operator=(const FakeKeyboard&) = delete;

  base::ScopedServiceBinding<fuchsia::ui::input3::Keyboard>* binding() {
    return &binding_;
  }

  // Sends |key_event| to |listener_|;
  void SendKeyEvent(KeyEvent key_event) {
    listener_->OnKeyEvent(std::move(key_event),
                          [num_sent_events = num_sent_events_,
                           this](fuchsia::ui::input3::KeyEventStatus status) {
                            ASSERT_EQ(num_acked_events_, num_sent_events)
                                << "Key events are acked out of order";
                            num_acked_events_++;
                          });
    num_sent_events_++;
  }

  // fuchsia::ui::input3::Keyboard implementation.
  void AddListener(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<::fuchsia::ui::input3::KeyboardListener> listener,
      AddListenerCallback callback) final {
    // This implementation is only set up to have up to one listener.
    DCHECK(!listener_);
    listener_ = listener.Bind();
    callback();
  }

  void NotImplemented_(const std::string& name) final {
    NOTIMPLEMENTED() << name;
  }

 private:
  fuchsia::ui::input3::KeyboardListenerPtr listener_;
  base::ScopedServiceBinding<fuchsia::ui::input3::Keyboard> binding_;

  // Counters to make sure key events are acked in order.
  int num_sent_events_ = 0;
  int num_acked_events_ = 0;
};

class KeyboardInputTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  KeyboardInputTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~KeyboardInputTest() override = default;

  KeyboardInputTest(const KeyboardInputTest&) = delete;
  KeyboardInputTest& operator=(const KeyboardInputTest&) = delete;

 protected:
  virtual void SetUpService() {
    keyboard_service_.emplace(component_context_->additional_services());
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kKeyboardInput}, {});
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    fuchsia::web::CreateFrameParams params;
    frame_for_test_ =
        cr_fuchsia::FrameForTest::Create(context(), std::move(params));

    // Set up services needed for the test. The keyboard service is included in
    // the allowed services by default. The real service needs to be removed so
    // it can be replaced by this fake implementation.
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    component_context_->additional_services()
        ->RemovePublicService<fuchsia::ui::input3::Keyboard>();
    SetUpService();
    virtual_keyboard_checker_.emplace(
        component_context_->additional_services());

    fuchsia::web::NavigationControllerPtr controller;
    frame_for_test_.ptr()->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/keyevents.html"));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    frame_for_test_.navigation_listener().RunUntilUrlEquals(test_url);

    fuchsia::web::FramePtr* frame_ptr = &(frame_for_test_.ptr());
    scenic_test_helper_.CreateScenicView(
        context_impl()->GetFrameImplForTest(frame_ptr), frame_for_test_.ptr());
    scenic_test_helper_.SetUpViewForInteraction(
        context_impl()->GetFrameImplForTest(frame_ptr)->web_contents());
  }

  // The tests expect to have input processed immediately, even if the
  // content has not been displayed yet. That's fine for the test, but
  // we need to explicitly allow it.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("allow-pre-commit-input");
  }

  template <typename... Args>
  void ExpectKeyEventsEqual(Args... events) {
    std::vector<base::Value> expected;
    AppendValueList(&expected, std::forward<Args>(events)...);
    frame_for_test_.navigation_listener().RunUntilTitleEquals(
        base::NumberToString(expected.size()));

    absl::optional<base::Value> actual =
        cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), kKeyDicts);
    EXPECT_EQ(*actual, base::Value(expected));
  }

  // Used to publish fake services.
  absl::optional<base::TestComponentContextForProcess> component_context_;

  cr_fuchsia::FrameForTest frame_for_test_;
  cr_fuchsia::ScenicTestHelper scenic_test_helper_;
  absl::optional<FakeKeyboard> keyboard_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  absl::optional<
      NeverConnectedChecker<fuchsia::input::virtualkeyboard::ControllerCreator>>
      virtual_keyboard_checker_;
};

// Check that printable keys are sent and received correctly.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, PrintableKeys) {
  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::A, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::KEY_8, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::KEY_8, KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::A, KeyEventType::RELEASED));

  ExpectKeyEventsEqual(ExpectedKeyValue("KeyA", "a", kKeyDown),
                       ExpectedKeyValue("KeyA", "a", kKeyPress),
                       ExpectedKeyValue("Digit8", "8", kKeyDown),
                       ExpectedKeyValue("Digit8", "8", kKeyPress),
                       ExpectedKeyValue("Digit8", "8", kKeyUp),
                       ExpectedKeyValue("KeyA", "a", kKeyUp));
}

// Check that character virtual keys are sent and received correctly.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, Characters) {
  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('A', KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('A', KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('b', KeyEventType::PRESSED));

  ExpectKeyEventsEqual(ExpectedKeyValue("", "A", kKeyPress),
                       ExpectedKeyValue("", "b", kKeyPress));
}

// Verify that character events are not affected by active modifiers.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftCharacter) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::LEFT_SHIFT, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('a', KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('a', KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::LEFT_SHIFT, KeyEventType::RELEASED));

  ExpectKeyEventsEqual(
      ExpectedKeyValue("ShiftLeft", "Shift", kKeyDown),
      ExpectedKeyValue("", "a", kKeyPress),  // Remains lowercase.
      ExpectedKeyValue("ShiftLeft", "Shift", kKeyUp));
}

// Verifies that codepoints outside the 16-bit Unicode BMP are rejected.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, CharacterInBmp) {
  const wchar_t kSigma = 0x03C3;
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent(kSigma, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent(kSigma, KeyEventType::RELEASED));

  std::string expected_utf8;
  ASSERT_TRUE(base::WideToUTF8(&kSigma, 1, &expected_utf8));
  ExpectKeyEventsEqual(ExpectedKeyValue("", expected_utf8, kKeyPress));
}

// Verifies that codepoints beyond the range of allowable UCS-2 values
// are rejected.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, CharacterBeyondBmp) {
  const uint32_t kRamenEmoji = 0x1F35C;

  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent(kRamenEmoji, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent(kRamenEmoji, KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('a', KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateCharacterEvent('a', KeyEventType::RELEASED));

  ExpectKeyEventsEqual(ExpectedKeyValue("", "a", kKeyPress));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftPrintableKeys) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::LEFT_SHIFT, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::B, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::KEY_1, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::SPACE, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::LEFT_SHIFT, KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::DOT, KeyEventType::PRESSED));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  ExpectKeyEventsEqual(ExpectedKeyValue("ShiftLeft", "Shift", kKeyDown),
                       ExpectedKeyValue("KeyB", "B", kKeyDown),
                       ExpectedKeyValue("KeyB", "B", kKeyPress),
                       ExpectedKeyValue("Digit1", "!", kKeyDown),
                       ExpectedKeyValue("Digit1", "!", kKeyPress),
                       ExpectedKeyValue("Space", " ", kKeyDown),
                       ExpectedKeyValue("Space", " ", kKeyPress),
                       ExpectedKeyValue("ShiftLeft", "Shift", kKeyUp),
                       ExpectedKeyValue("Period", ".", kKeyDown),
                       ExpectedKeyValue("Period", ".", kKeyPress));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftNonPrintableKeys) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::RIGHT_SHIFT, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::ENTER, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::LEFT_CTRL, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::RIGHT_SHIFT, KeyEventType::RELEASED));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  ExpectKeyEventsEqual(ExpectedKeyValue("ShiftRight", "Shift", kKeyDown),
                       ExpectedKeyValue("Enter", "Enter", kKeyDown),
                       ExpectedKeyValue("Enter", "Enter", kKeyPress),
                       ExpectedKeyValue("ControlLeft", "Control", kKeyDown),
                       ExpectedKeyValue("ShiftRight", "Shift", kKeyUp));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, Disconnect) {
  // Disconnect the keyboard service.
  keyboard_service_.reset();

  frame_for_test_.navigation_listener().RunUntilTitleEquals("loaded");

  // Make sure the page is still available and there are no crashes.
  EXPECT_TRUE(cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), "true")
                  ->GetBool());
}

class KeyboardInputTestWithoutKeyboardFeature : public KeyboardInputTest {
 public:
  KeyboardInputTestWithoutKeyboardFeature() = default;
  ~KeyboardInputTestWithoutKeyboardFeature() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({}, {});
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpService() override {
    keyboard_input_checker_.emplace(component_context_->additional_services());
  }

  absl::optional<NeverConnectedChecker<fuchsia::ui::input3::Keyboard>>
      keyboard_input_checker_;
};

IN_PROC_BROWSER_TEST_F(KeyboardInputTestWithoutKeyboardFeature, NoFeature) {
  // Test will verify that |keyboard_input_checker_| never received a connection
  // request at teardown time.
}

}  // namespace
