// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <string>

#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::AudioEncoderApiInvocationSequence& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

  // Some Image related classes that use base::Singleton will expect this to
  // exist for registering exit callbacks (e.g. DarkModeImageClassifier).
  base::AtExitManager exit_manager;

  //
  // NOTE: GC objects that need to survive iterations of the loop below
  // must be Persistent<>!
  //
  // GC may be triggered by the RunLoop().RunUntilIdle() below, which will GC
  // raw pointers on the stack. This is not required in production code because
  // GC typically runs at the top of the stack, or is conservative enough to
  // keep stack pointers alive.
  //

  // Scoping Persistent<> refs so GC can collect these at the end.
  {
    Persistent<ScriptState> script_state =
        ToScriptStateForMainWorld(&page_holder->GetFrame());
    ScriptState::Scope scope(script_state);

    Persistent<FakeFunction> error_function =
        FakeFunction::Create(script_state, "error");
    Persistent<V8WebCodecsErrorCallback> error_callback =
        V8WebCodecsErrorCallback::Create(error_function->Bind());
    Persistent<FakeFunction> output_function =
        FakeFunction::Create(script_state, "output");
    Persistent<V8EncodedAudioChunkOutputCallback> output_callback =
        V8EncodedAudioChunkOutputCallback::Create(output_function->Bind());

    Persistent<AudioEncoderInit> audio_encoder_init =
        MakeGarbageCollected<AudioEncoderInit>();
    audio_encoder_init->setError(error_callback);
    audio_encoder_init->setOutput(output_callback);

    Persistent<AudioEncoder> audio_encoder = AudioEncoder::Create(
        script_state, audio_encoder_init, IGNORE_EXCEPTION_FOR_TESTING);

    if (audio_encoder) {
      for (auto& invocation : proto.invocations()) {
        switch (invocation.Api_case()) {
          case wc_fuzzer::AudioEncoderApiInvocation::kConfigure: {
            AudioEncoderConfig* config =
                MakeAudioEncoderConfig(invocation.configure());

            // Use the same config to fuzz isConfigSupported().
            AudioEncoder::isConfigSupported(script_state, config,
                                            IGNORE_EXCEPTION_FOR_TESTING);

            audio_encoder->configure(config, IGNORE_EXCEPTION_FOR_TESTING);
            break;
          }
          case wc_fuzzer::AudioEncoderApiInvocation::kEncode: {
            AudioData* data =
                MakeAudioData(script_state, invocation.encode().data());
            if (!data)
              return;

            audio_encoder->encode(data, IGNORE_EXCEPTION_FOR_TESTING);
            break;
          }
          case wc_fuzzer::AudioEncoderApiInvocation::kFlush: {
            // TODO(https://crbug.com/1119253): Fuzz whether to await resolution
            // of the flush promise.
            audio_encoder->flush(IGNORE_EXCEPTION_FOR_TESTING);
            break;
          }
          case wc_fuzzer::AudioEncoderApiInvocation::kReset:
            audio_encoder->reset(IGNORE_EXCEPTION_FOR_TESTING);
            break;
          case wc_fuzzer::AudioEncoderApiInvocation::kClose:
            audio_encoder->close(IGNORE_EXCEPTION_FOR_TESTING);
            break;
          case wc_fuzzer::AudioEncoderApiInvocation::API_NOT_SET:
            break;
        }

        // Give other tasks a chance to run (e.g. calling our output callback).
        base::RunLoop().RunUntilIdle();
      }
    }
  }

  // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
  //
  // Multiple GCs may be required to ensure everything is collected (due to
  // a chain of persistent handles), so some objects may not be collected until
  // a subsequent iteration. This is slow enough as is, so we compromise on one
  // major GC, as opposed to the 5 used in V8GCController for unit tests.
  V8PerIsolateData::MainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
