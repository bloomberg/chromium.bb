// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/system_engine.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

constexpr char kImeSpec[] = "xkb:us::eng";

class TestDecoderState;

// The fake decoder state has to be available globally because
// ImeDecoder::EntryPoints is a list of stateless C functions, so the only way
// to have a stateful fake is to have a global reference to it.
TestDecoderState* g_test_decoder_state = nullptr;

mojo::ScopedMessagePipeHandle MessagePipeHandleFromInt(uint32_t handle) {
  return mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handle));
}

struct MockInputMethod : public mojom::InputMethod {
  MOCK_METHOD(void,
              OnFocus,
              (chromeos::ime::mojom::InputFieldInfoPtr input_field_info),
              (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void,
              OnSurroundingTextChanged,
              (const std::string& text,
               uint32_t offset,
               chromeos::ime::mojom::SelectionRangePtr selection_range),
              (override));
  MOCK_METHOD(void, OnCompositionCanceledBySystem, (), (override));
  MOCK_METHOD(void,
              ProcessKeyEvent,
              (chromeos::ime::mojom::PhysicalKeyEventPtr event,
               ProcessKeyEventCallback callback),
              (override));
};

class TestDecoderState {
 public:
  bool ConnectToInputMethod(const char* ime_spec,
                            uint32_t receiver_pipe_handle,
                            uint32_t host_pipe_handle,
                            uint32_t host_pipe_version) {
    receiver.Bind(mojo::PendingReceiver<mojom::InputMethod>(
        MessagePipeHandleFromInt(receiver_pipe_handle)));
    input_method_host.Bind(mojo::PendingRemote<mojom::InputMethodHost>(
        MessagePipeHandleFromInt(host_pipe_handle), host_pipe_version));
    return true;
  }

  MockInputMethod mock_input_method;
  mojo::Receiver<mojom::InputMethod> receiver{&mock_input_method};
  mojo::Remote<mojom::InputMethodHost> input_method_host;
};

ImeDecoder::EntryPoints CreateDecoderEntryPoints(TestDecoderState* state) {
  g_test_decoder_state = state;

  ImeDecoder::EntryPoints entry_points;
  entry_points.init_once = [](ImeCrosPlatform* platform) {};
  entry_points.connect_to_input_method = [](const char* ime_spec,
                                            uint32_t receiver_pipe_handle,
                                            uint32_t host_pipe_handle,
                                            uint32_t host_pipe_version) {
    return g_test_decoder_state->ConnectToInputMethod(
        ime_spec, receiver_pipe_handle, host_pipe_handle, host_pipe_version);
  };
  entry_points.is_input_method_connected = []() { return false; };
  return entry_points;
}

struct MockInputMethodHost : public ime::mojom::InputMethodHost {
  MOCK_METHOD(void,
              CommitText,
              (const std::u16string& text,
               mojom::CommitTextCursorBehavior cursor_behavior),
              (override));
  MOCK_METHOD(void,
              SetComposition,
              (const std::u16string& text,
               std::vector<mojom::CompositionSpanPtr> spans),
              (override));
  MOCK_METHOD(void,
              SetCompositionRange,
              (uint32_t start_index, uint32_t end_index),
              (override));
  MOCK_METHOD(void, FinishComposition, (), (override));
  MOCK_METHOD(void,
              DeleteSurroundingText,
              (uint32_t num_before_cursor, uint32_t num_after_cursor),
              (override));
  MOCK_METHOD(void,
              HandleAutocorrect,
              (mojom::AutocorrectSpanPtr autocorrect_span),
              (override));
  MOCK_METHOD(void,
              RequestSuggestions,
              (mojom::SuggestionsRequestPtr request,
               RequestSuggestionsCallback callback),
              (override));
  MOCK_METHOD(void,
              DisplaySuggestions,
              (const std::vector<ime::TextSuggestion>& suggestions),
              (override));
  MOCK_METHOD(void, RecordUkm, (mojom::UkmEntryPtr entry), (override));
};

// Sets up the test environment for Mojo and inject a mock ImeEngineMainEntry.
class SystemEngineTest : public testing::Test {
 protected:
  void SetUp() final {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kSystemLatinPhysicalTyping}, {});
  }

 private:
  // Mojo calls need a SequencedTaskRunner.
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SystemEngineTest, BindRequestConnectsInputMethod) {
  TestDecoderState state;
  FakeDecoderEntryPointsForTesting(CreateDecoderEntryPoints(&state));
  SystemEngine engine(/*platform=*/nullptr);

  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> receiver{&mock_host};
  EXPECT_TRUE(engine.BindRequest(kImeSpec,
                                 input_method.BindNewPipeAndPassReceiver(),
                                 receiver.BindNewPipeAndPassRemote()));

  ASSERT_TRUE(input_method.is_bound());
  EXPECT_TRUE(input_method.is_connected());
}

TEST_F(SystemEngineTest, CanSendMessagesAfterBinding) {
  TestDecoderState state;
  FakeDecoderEntryPointsForTesting(CreateDecoderEntryPoints(&state));
  SystemEngine engine(/*platform=*/nullptr);

  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> receiver{&mock_host};
  EXPECT_TRUE(engine.BindRequest(kImeSpec,
                                 input_method.BindNewPipeAndPassReceiver(),
                                 receiver.BindNewPipeAndPassRemote()));

  EXPECT_CALL(state.mock_input_method, OnBlur);

  // Send any Mojo message to the input method.
  input_method->OnBlur();
  input_method.FlushForTesting();
}

TEST_F(SystemEngineTest, CanReceiveMessagesAfterBinding) {
  TestDecoderState state;
  FakeDecoderEntryPointsForTesting(CreateDecoderEntryPoints(&state));
  SystemEngine engine(/*platform=*/nullptr);

  mojo::Remote<mojom::InputMethod> input_method;
  MockInputMethodHost mock_host;
  mojo::Receiver<mojom::InputMethodHost> receiver{&mock_host};
  EXPECT_TRUE(engine.BindRequest(kImeSpec,
                                 input_method.BindNewPipeAndPassReceiver(),
                                 receiver.BindNewPipeAndPassRemote()));

  EXPECT_CALL(mock_host, FinishComposition);

  // Send any Mojo message from the input method.
  state.input_method_host->FinishComposition();
  state.input_method_host.FlushForTesting();
}

}  // namespace ime
}  // namespace chromeos
