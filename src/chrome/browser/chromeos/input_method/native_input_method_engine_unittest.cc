// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

MATCHER_P(MojoEq, value, "") {
  return *arg == value;
}

using input_method::InputMethodManager;
using input_method::StubInputMethodEngineObserver;
using testing::_;
using testing::NiceMock;
using testing::StrictMock;

constexpr char kEngineIdUs[] = "xkb:us::eng";

class MockInputMethod : public ime::mojom::InputMethod {
 public:
  void Bind(mojo::PendingReceiver<ime::mojom::InputMethod> receiver,
            mojo::PendingRemote<ime::mojom::InputMethodHost> pending_host) {
    receiver_.Bind(std::move(receiver));
    host.Bind(std::move(pending_host));
  }

  // ime::mojom::InputMethod:
  MOCK_METHOD(void,
              OnFocus,
              (ime::mojom::InputFieldInfoPtr input_field_info),
              (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void,
              ProcessKeyEvent,
              (const ime::mojom::PhysicalKeyEventPtr event,
               ProcessKeyEventCallback),
              (override));
  MOCK_METHOD(void,
              OnSurroundingTextChanged,
              (const std::string& text,
               uint32_t offset,
               ime::mojom::SelectionRangePtr selection_range),
              (override));
  MOCK_METHOD(void, OnCompositionCanceledBySystem, (), (override));

  mojo::Remote<ime::mojom::InputMethodHost> host;

 private:
  mojo::Receiver<ime::mojom::InputMethod> receiver_{this};
};

void SetPhysicalTypingAutocorrectEnabled(Profile& profile, bool enabled) {
  base::Value input_method_setting(base::Value::Type::DICTIONARY);
  input_method_setting.SetPath(
      std::string(kEngineIdUs) + ".physicalKeyboardAutoCorrectionLevel",
      base::Value(enabled ? 1 : 0));
  profile.GetPrefs()->Set(prefs::kLanguageInputMethodSpecificSettings,
                          input_method_setting);
}

class TestInputEngineManager : public ime::mojom::InputEngineManager {
 public:
  explicit TestInputEngineManager(MockInputMethod* mock_input_method)
      : mock_input_method_(mock_input_method) {}

  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<ime::mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<ime::mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override {
    // Not used by NativeInputMethodEngine.
    std::move(callback).Run(/*bound=*/false);
  }

  void ConnectToInputMethod(
      const std::string& ime_spec,
      mojo::PendingReceiver<ime::mojom::InputMethod> input_method,
      mojo::PendingRemote<ime::mojom::InputMethodHost> host,
      ConnectToInputMethodCallback callback) override {
    mock_input_method_->Bind(std::move(input_method), std::move(host));
    std::move(callback).Run(/*bound=*/true);
  }

 private:
  MockInputMethod* mock_input_method_;
};

class TestInputMethodManager : public input_method::MockInputMethodManager {
 public:
  // TestInputMethodManager is responsible for connecting
  // NativeInputMethodEngine with an InputMethod.
  explicit TestInputMethodManager(MockInputMethod* mock_input_method)
      : test_input_engine_manager_(mock_input_method),
        receiver_(&test_input_engine_manager_) {}

  void ConnectInputEngineManager(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) override {
    receiver_.Bind(std::move(receiver));
  }

 private:
  TestInputEngineManager test_input_engine_manager_;
  mojo::Receiver<ime::mojom::InputEngineManager> receiver_;
};

// TODO(crbug.com/1148157): Refactor NativeInputMethodEngine etc. to avoid
// hidden dependencies on globals such as ImeBridge.
class NativeInputMethodEngineTest : public ::testing::Test {
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistPersonalInfo,
                              features::kAssistPersonalInfoEmail,
                              features::kAssistPersonalInfoName,
                              features::kEmojiSuggestAddition,
                              features::kImeMojoDecoder,
                              features::kSystemLatinPhysicalTyping},
        /*disabled_features=*/{});

    // Needed by NativeInputMethodEngine to interact with the input field.
    ui::IMEBridge::Initialize();

    // Needed by NativeInputMethodEngine for the virtual keyboard.
    keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_test_helper_;
  machine_learning::FakeServiceConnectionImpl fake_service_connection_;
};

TEST_F(NativeInputMethodEngineTest, DoesNotLaunchImeServiceIfAutocorrectIsOff) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, LaunchesImeServiceIfAutocorrectIsOn) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  EXPECT_TRUE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, TogglesImeServiceWhenAutocorrectChanges) {
  TestingProfile testing_profile;
  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  engine.Enable(kEngineIdUs);

  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);
  EXPECT_TRUE(engine.IsConnectedForTesting());
  SetPhysicalTypingAutocorrectEnabled(testing_profile, false);
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, EnableInitializesConnection) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  EXPECT_TRUE(engine.IsConnectedForTesting());
  EXPECT_TRUE(mock_input_method.host.is_bound());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, FocusCallsRightMojoFunctions) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                    ime::mojom::InputFieldType::kText,
                    ime::mojom::AutocorrectMode::kEnabled,
                    ime::mojom::PersonalizationMode::kEnabled))));
  }

  ui::IMEEngineHandlerInterface::InputContext input_context(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true);
  engine.Enable(kEngineIdUs);
  engine.FocusIn(input_context);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, HandleAutocorrectChangesAutocorrectRange) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::NiceMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  ui::IMEEngineHandlerInterface::InputContext input_context(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true);
  engine.Enable(kEngineIdUs);
  engine.FocusIn(input_context);
  engine.FlushForTesting();
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);

  mock_input_method.host->HandleAutocorrect(
      ime::mojom::AutocorrectSpan::New(gfx::Range(0, 5), u"teh", u"the"));
  mock_input_method.host.FlushForTesting();

  EXPECT_EQ(mock_handler.GetAutocorrectRange(), gfx::Range(0, 5));

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       SurroundingTextChangeConvertsToUtf8Correctly) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_));
    // Each character in "你好" is three UTF-8 code units.
    EXPECT_CALL(mock_input_method,
                OnSurroundingTextChanged(u8"你好",
                                         /*offset=*/0,
                                         MojoEq(ime::mojom::SelectionRange(
                                             /*anchor=*/6, /*focus=*/6))));
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));
  // Each character in "你好" is one UTF-16 code unit.
  engine.SetSurroundingText(u"你好",
                            /*cursor_pos=*/2,
                            /*anchor_pos=*/2,
                            /*offset=*/0);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, ProcessesDeadKeysCorrectly) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_));

    // TODO(https://crbug.com/1187982): Expect the actual arguments to the call
    // once the Mojo API is replaced with protos. GMock does not play well with
    // move-only types like PhysicalKeyEvent.
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _))
        .Times(2)
        .WillRepeatedly(::testing::Invoke(
            [](ime::mojom::PhysicalKeyEventPtr,
               ime::mojom::InputMethod::ProcessKeyEventCallback callback) {
              std::move(callback).Run(
                  ime::mojom::KeyEventResult::kNeedsHandlingBySystem);
            }));
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));

  // Quote ("VKEY_OEM_7") + A is a dead key combination.
  engine.ProcessKeyEvent(
      {ui::ET_KEY_PRESSED, ui::VKEY_OEM_7, ui::DomCode::QUOTE, ui::EF_NONE,
       ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'), base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::ET_KEY_RELEASED, ui::VKEY_OEM_7, ui::DomCode::QUOTE, ui::EF_NONE,
       ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'), base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_RELEASED, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, ProcessesNamedKeysCorrectly) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_));

    // TODO(https://crbug.com/1187982): Expect the actual arguments to the call
    // once the Mojo API is replaced with protos. GMock does not play well with
    // move-only types like PhysicalKeyEvent.
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _))
        .Times(4)
        .WillRepeatedly(::testing::Invoke(
            [](ime::mojom::PhysicalKeyEventPtr event,
               ime::mojom::InputMethod::ProcessKeyEventCallback callback) {
              EXPECT_TRUE(event->key->is_named_key());
              std::move(callback).Run(
                  ime::mojom::KeyEventResult::kNeedsHandlingBySystem);
            }));
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));

  // Enter and Backspace are named keys with Unicode representation.
  engine.ProcessKeyEvent(
      {ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::DomCode::ENTER, ui::EF_NONE,
       ui::DomKey::ENTER, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_RELEASED, ui::VKEY_RETURN, ui::EF_NONE},
                         base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::ET_KEY_PRESSED, ui::VKEY_BACK, ui::DomCode::BACKSPACE, ui::EF_NONE,
       ui::DomKey::BACKSPACE, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_RELEASED, ui::VKEY_BACK, ui::EF_NONE},
                         base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, DoesNotSendUnhandledNamedKeys) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_));
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _)).Times(0);
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));

  // Escape is a named DOM key, but is not used by IMEs.
  engine.ProcessKeyEvent(
      {ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::EF_NONE,
       ui::DomKey::ESCAPE, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_RELEASED, ui::VKEY_ESCAPE, ui::EF_NONE},
                         base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

// TODO(crbug.com/1148157): Refactor NativeInputMethodEngine etc. to avoid
// hidden dependencies on globals such as ImeBridge.
class NativeInputMethodEngineWithRenderViewHostTest
    : public content::RenderViewHostTestHarness {
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistPersonalInfo,
                              features::kAssistPersonalInfoEmail,
                              features::kAssistPersonalInfoName,
                              features::kEmojiSuggestAddition,
                              features::kImeMojoDecoder,
                              features::kSystemLatinPhysicalTyping},
        /*disabled_features=*/{});

    // Needed by NativeInputMethodEngine to interact with the input field.
    ui::IMEBridge::Initialize();

    // Needed by NativeInputMethodEngine for the virtual keyboard.
    keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_test_helper_;
  machine_learning::FakeServiceConnectionImpl fake_service_connection_;
};

TEST_F(NativeInputMethodEngineWithRenderViewHostTest,
       RecordUkmAddsNonCompliantApiUkmEntry) {
  GURL url("https://www.example.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  auto* testing_profile = static_cast<TestingProfile*>(browser_context());
  SetPhysicalTypingAutocorrectEnabled(*testing_profile, true);

  testing::NiceMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", testing_profile);
  ui::IMEEngineHandlerInterface::InputContext input_context(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true);
  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.set_source_id(
      ukm::GetSourceIdForWebContentsDocument(web_contents()));

  ui::InputMethodChromeOS ime(nullptr);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.EnableRecording(false /* extensions */);
  ASSERT_EQ(0u, test_recorder.entries_count());

  auto entry = ime::mojom::UkmEntry::New();
  auto metric = ime::mojom::NonCompliantApiMetric::New();
  metric->non_compliant_operation =
      ime::mojom::InputMethodApiOperation::kSetCompositionText;
  entry->set_non_compliant_api(std::move(metric));
  mock_input_method.host->RecordUkm(std::move(entry));
  mock_input_method.host.FlushForTesting();

  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(1u, test_recorder.entries_count());
  const auto entries =
      test_recorder.GetEntriesByName("InputMethod.NonCompliantApi");
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], "NonCompliantOperation", 1);  // kSetCompositionText

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineWithRenderViewHostTest,
       RecordUkmAddsAssistiveMatchUkmEntry) {
  GURL url("https://www.example.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  auto* testing_profile = static_cast<TestingProfile*>(browser_context());
  testing::NiceMock<MockInputMethod> mock_input_method;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", testing_profile);

  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.set_source_id(
      ukm::GetSourceIdForWebContentsDocument(web_contents()));

  ui::InputMethodChromeOS ime(nullptr);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  ui::IMEBridge::Get()->SetInputContextHandler(&ime);

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.EnableRecording(false /* extensions */);
  ASSERT_EQ(0u, test_recorder.entries_count());

  // Should not record when random text is entered.
  engine.SetSurroundingText(u"random text ", 12, 12, 0);
  EXPECT_EQ(0u, test_recorder.entries_count());

  // Should record when match is triggered.
  engine.SetSurroundingText(u"my email is ", 12, 12, 0);
  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(1u, test_recorder.entries_count());
  const auto entries =
      test_recorder.GetEntriesByName("InputMethod.Assistive.Match");
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], "Type", (int)AssistiveType::kPersonalEmail);

  InputMethodManager::Shutdown();
}

}  // namespace
}  // namespace chromeos
