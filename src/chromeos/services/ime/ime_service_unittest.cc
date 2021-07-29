// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/services/ime/ime_decoder.h"
#include "chromeos/services/ime/mock_input_channel.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
constexpr char kValidImeSpec[] = "valid_spec";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

class TestDecoderState;

// The fake decoder state has to be available globally because
// ImeDecoder::EntryPoints is a list of stateless C functions, so the only way
// to have a stateful fake is to have a global reference to it.
TestDecoderState* g_test_decoder_state = nullptr;

mojo::ScopedMessagePipeHandle MessagePipeHandleFromInt(uint32_t handle) {
  return mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle));
}

class TestDecoderState : public mojom::InputMethod {
 public:
  bool ConnectToInputMethod(const char* ime_spec,
                            uint32_t receiver_pipe_handle,
                            uint32_t host_pipe_handle,
                            uint32_t host_pipe_version) {
    receiver_.reset();
    receiver_.Bind(mojo::PendingReceiver<mojom::InputMethod>(
        MessagePipeHandleFromInt(receiver_pipe_handle)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&mojo::Receiver<mojom::InputMethod>::reset,
                       base::Unretained(&receiver_)));

    input_method_host_.reset();
    input_method_host_.Bind(mojo::PendingRemote<mojom::InputMethodHost>(
        MessagePipeHandleFromInt(host_pipe_handle), host_pipe_version));
    return true;
  }
  bool IsInputMethodConnected() {
    // The receiver resets upon disconnection, so we can just check whether it
    // is bound.
    return receiver_.is_bound();
  }

 private:
  // mojom::InputMethod:
  void OnFocus(
      chromeos::ime::mojom::InputFieldInfoPtr input_field_info) override {}
  void OnBlur() override {}
  void OnSurroundingTextChanged(
      const std::string& text,
      uint32_t offset,
      chromeos::ime::mojom::SelectionRangePtr selection_range) override {}
  void OnCompositionCanceledBySystem() override {}
  void ProcessKeyEvent(chromeos::ime::mojom::PhysicalKeyEventPtr event,
                       ProcessKeyEventCallback callback) override {}

  mojo::Receiver<mojom::InputMethod> receiver_{this};
  mojo::Remote<mojom::InputMethodHost> input_method_host_;
};

ImeDecoder::EntryPoints CreateDecoderEntryPoints(TestDecoderState* state) {
  g_test_decoder_state = state;

  ImeDecoder::EntryPoints entry_points;
  entry_points.init_once = [](ImeCrosPlatform* platform) {};
  entry_points.supports = [](const char* ime_spec) {
    return strcmp(kInvalidImeSpec, ime_spec) != 0;
  };
  entry_points.activate_ime = [](const char* ime_spec,
                                 ImeClientDelegate* delegate) { return true; };
  entry_points.process = [](const uint8_t* data, size_t size) {};
  entry_points.connect_to_input_method = [](const char* ime_spec,
                                            uint32_t receiver_pipe_handle,
                                            uint32_t host_pipe_handle,
                                            uint32_t host_pipe_version) {
    return g_test_decoder_state->ConnectToInputMethod(
        ime_spec, receiver_pipe_handle, host_pipe_handle, host_pipe_version);
  };
  entry_points.is_input_method_connected = []() {
    return g_test_decoder_state->IsInputMethodConnected();
  };
  entry_points.close = []() {};
  return entry_points;
}

struct MockInputMethodHost : public mojom::InputMethodHost {
  void CommitText(const std::u16string& text,
                  mojom::CommitTextCursorBehavior cursor_behavior) override {
    last_commit = text;
  }
  void SetComposition(const std::u16string& text,
                      std::vector<mojom::CompositionSpanPtr> spans) override {
    last_composition = text;
  }
  void SetCompositionRange(uint32_t start_index, uint32_t end_index) override {}
  void FinishComposition() override {}
  void DeleteSurroundingText(uint32_t num_before_cursor,
                             uint32_t num_after_cursor) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(
      const std::vector<TextSuggestion>& suggestions) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}

  std::u16string last_commit;
  std::u16string last_composition;
};

class ImeServiceTest : public testing::Test, public mojom::InputMethodHost {
 public:
  ImeServiceTest() : service_(remote_service_.BindNewPipeAndPassReceiver()) {}
  ~ImeServiceTest() override = default;

  void CommitText(const std::u16string& text,
                  mojom::CommitTextCursorBehavior cursor_behavior) override {}
  void SetComposition(const std::u16string& text,
                      std::vector<mojom::CompositionSpanPtr> spans) override {}
  void SetCompositionRange(uint32_t start_index, uint32_t end_index) override {}
  void FinishComposition() override {}
  void DeleteSurroundingText(uint32_t num_before_cursor,
                             uint32_t num_after_cursor) override {}
  void HandleAutocorrect(mojom::AutocorrectSpanPtr autocorrect_span) override {}
  void RequestSuggestions(mojom::SuggestionsRequestPtr request,
                          RequestSuggestionsCallback callback) override {}
  void DisplaySuggestions(const std::vector<::chromeos::ime::TextSuggestion>&
                              suggestions) override {}
  void RecordUkm(mojom::UkmEntryPtr entry) override {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kImeMojoDecoder,
                              features::kSystemLatinPhysicalTyping},
        /*disabled_features=*/{});

    FakeDecoderEntryPointsForTesting(CreateDecoderEntryPoints(&state_));
    remote_service_->BindInputEngineManager(
        remote_manager_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::ImeService> remote_service_;
  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ImeService service_;
  TestDecoderState state_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngineDoesNotConnectRemote) {
  bool success = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kInvalidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  EXPECT_FALSE(success);
  EXPECT_FALSE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToValidEngineConnectsRemote) {
  bool success = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), {},
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success);
  EXPECT_TRUE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToImeEngineWillOverrideExistingImeEngine) {
  bool success1, success2 = true;
  MockInputChannel test_channel1, test_channel2;
  mojo::Remote<mojom::InputChannel> remote_engine1, remote_engine2;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine1.BindNewPipeAndPassReceiver(),
      test_channel1.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine2.BindNewPipeAndPassReceiver(),
      test_channel2.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_FALSE(remote_engine1.is_connected());
  EXPECT_TRUE(remote_engine2.is_connected());
}

TEST_F(ImeServiceTest,
       ConnectToImeEngineCannotConnectIfInputMethodIsConnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have failed.
  EXPECT_TRUE(success1);
  EXPECT_FALSE(success2);
  EXPECT_TRUE(input_method.is_connected());
  EXPECT_FALSE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest,
       ConnectToImeEngineCanConnectIfInputMethodIsDisconnected) {
  bool success1, success2 = true;
  MockInputChannel test_channel;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success1));
  input_method.reset();
  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_.FlushForTesting();

  // The second connection should have succeed since the first connection was
  // disconnected.
  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_FALSE(input_method.is_bound());
  EXPECT_TRUE(remote_engine.is_connected());
}

TEST_F(ImeServiceTest, ConnectToInputMethodCanOverrideAnyConnection) {
  bool success1, success2, success3 = true;
  MockInputChannel test_channel;
  mojo::Receiver<mojom::InputMethodHost> host1(this), host2(this);
  mojo::Remote<mojom::InputMethod> input_method1, input_method2;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kValidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), /*extra=*/{},
      base::BindOnce(&ConnectCallback, &success1));
  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method1.BindNewPipeAndPassReceiver(),
      host1.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success2));
  remote_manager_->ConnectToInputMethod(
      kValidImeSpec, input_method2.BindNewPipeAndPassReceiver(),
      host2.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success3));
  remote_manager_.FlushForTesting();

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);
  EXPECT_FALSE(remote_engine.is_connected());
  EXPECT_FALSE(input_method1.is_connected());
  EXPECT_TRUE(input_method2.is_connected());
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleModifierKeys) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  constexpr std::pair<mojom::NamedDomKey, mojom::DomCode> kModifierKeys[] = {
      {mojom::NamedDomKey::kShift, mojom::DomCode::kShiftLeft},
      {mojom::NamedDomKey::kShift, mojom::DomCode::kShiftRight},
      {mojom::NamedDomKey::kAlt, mojom::DomCode::kAltLeft},
      {mojom::NamedDomKey::kAlt, mojom::DomCode::kAltRight},
      {mojom::NamedDomKey::kAltGraph, mojom::DomCode::kAltRight},
      {mojom::NamedDomKey::kCapsLock, mojom::DomCode::kCapsLock},
      {mojom::NamedDomKey::kControl, mojom::DomCode::kControlLeft},
      {mojom::NamedDomKey::kControl, mojom::DomCode::kControlRight}};
  for (const auto& modifier_key : kModifierKeys) {
    input_method->ProcessKeyEvent(
        mojom::PhysicalKeyEvent::New(
            mojom::KeyEventType::kKeyDown,
            mojom::DomKey::NewNamedKey(modifier_key.first), modifier_key.second,
            mojom::ModifierState::New()),
        base::BindLambdaForTesting([](mojom::KeyEventResult result) {
          EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
        }));
    input_method.FlushForTesting();
  }
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleCtrlShortCut) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kControl),
          mojom::DomCode::kControlLeft,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_control = mojom::ModifierState::New();
  modifier_state_with_control->control = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_control->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();
}

TEST_F(ImeServiceTest, RuleBasedDoesNotHandleAltShortCut) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  mojo::Receiver<mojom::InputMethodHost> host(this);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltLeft,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto new_modifier_state = mojom::ModifierState::New();
  new_modifier_state->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, std::move(new_modifier_state)),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();
}

TEST_F(ImeServiceTest, RuleBasedHandlesAltRight) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltRight,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_alt->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_FALSE(mock_host.last_commit.empty());
      }));
  input_method.FlushForTesting();
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->ConnectToInputMethod(
      "m17n:ar", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('A'),
          mojom::DomCode::kKeyA, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\u0650");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Test KeyB
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('b'),
          mojom::DomCode::kKeyB, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\u0644\u0627");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Test unhandled key.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kEnter),
          mojom::DomCode::kEnter,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();

  // Test keyup.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyUp,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kEnter),
          mojom::DomCode::kEnter,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));
  input_method.FlushForTesting();

  // TODO(keithlee) Test reset function
  input_method->OnCompositionCanceledBySystem();
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test KeyN.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('n'),
          mojom::DomCode::kKeyN, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_TRUE(mock_host.last_commit.empty());
        EXPECT_EQ(mock_host.last_composition, u"\u0928");
      }));
  input_method.FlushForTesting();

  // Backspace.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kBackspace),
          mojom::DomCode::kBackspace,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_TRUE(mock_host.last_commit.empty());
        EXPECT_EQ(mock_host.last_composition, u"");
      }));
  input_method.FlushForTesting();

  // KeyN + KeyC.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('n'),
          mojom::DomCode::kKeyN, mojom::ModifierState::New()),
      base::DoNothing());
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('c'),
          mojom::DomCode::kKeyC, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_composition, u"\u091e\u094d\u091a");
      }));
  input_method.FlushForTesting();

  // Space.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint(' '),
          mojom::DomCode::kSpace, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_composition, u"\u091e\u094d\u091a");
      }));
  input_method.FlushForTesting();
}

// Tests escapable characters. See https://crbug.com/1014384.
TEST_F(ImeServiceTest, RuleBasedDoesNotEscapeCharacters) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->ConnectToInputMethod(
      "m17n:deva_phone", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  auto modifier_state_with_shift = mojom::ModifierState::New();
  modifier_state_with_shift->shift = true;

  // Test Shift+Quote ('"').
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('"'),
          mojom::DomCode::kQuote, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\"");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Backslash.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('\\'),
          mojom::DomCode::kBackslash, mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"\\");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();

  // Shift+Comma ('<')
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('<'),
          mojom::DomCode::kComma, modifier_state_with_shift->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"<");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();
}

// Tests that AltGr works with rule-based. See crbug.com/1035145.
TEST_F(ImeServiceTest, KhmerKeyboardAltGr) {
  bool success = false;
  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> host(&mock_host);

  remote_manager_->ConnectToInputMethod(
      "m17n:km", input_method.BindNewPipeAndPassReceiver(),
      host.BindNewPipeAndPassRemote(),
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test AltRight+KeyA.
  // We do not support AltGr for rule-based. We treat AltRight as AltGr.
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown,
          mojom::DomKey::NewNamedKey(mojom::NamedDomKey::kAlt),
          mojom::DomCode::kAltRight,

          mojom::ModifierState::New()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kNeedsHandlingBySystem);
      }));

  auto modifier_state_with_alt = mojom::ModifierState::New();
  modifier_state_with_alt->alt = true;
  input_method->ProcessKeyEvent(
      mojom::PhysicalKeyEvent::New(
          mojom::KeyEventType::kKeyDown, mojom::DomKey::NewCodepoint('a'),
          mojom::DomCode::kKeyA, modifier_state_with_alt->Clone()),
      base::BindLambdaForTesting([&](mojom::KeyEventResult result) {
        EXPECT_EQ(result, mojom::KeyEventResult::kConsumedByIme);
        EXPECT_EQ(mock_host.last_commit, u"+");
        EXPECT_TRUE(mock_host.last_composition.empty());
      }));
  input_method.FlushForTesting();
}

}  // namespace ime
}  // namespace chromeos
