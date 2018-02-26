// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletGlobalScope.h"

#include <memory>

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/ScriptModule.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/messaging/MessageChannel.h"
#include "core/messaging/MessagePort.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/testing/PageTestBase.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerReportingProxy.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletProcessorDefinition.h"
#include "modules/webaudio/AudioWorkletThread.h"
#include "platform/audio/AudioBus.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/TextPosition.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebURLRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static const size_t kRenderQuantumFrames = 128;

}  // namespace

class AudioWorkletGlobalScopeTest : public PageTestBase {
 public:
  void SetUp() override {
    AudioWorkletThread::CreateSharedBackingThreadForTest();
    PageTestBase::SetUp(IntSize());
    Document* document = &GetDocument();
    document->SetURL(KURL("https://example.com/"));
    document->UpdateSecurityOrigin(SecurityOrigin::Create(document->Url()));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  std::unique_ptr<AudioWorkletThread> CreateAudioWorkletThread() {
    std::unique_ptr<AudioWorkletThread> thread =
        AudioWorkletThread::Create(nullptr, *reporting_proxy_);
    Document* document = &GetDocument();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), document->UserAgent(),
            nullptr /* content_security_policy_parsed_headers */,
            document->GetReferrerPolicy(), document->GetSecurityOrigin(),
            document->IsSecureContext(), nullptr /* worker_clients */,
            document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            kV8CacheOptionsDefault),
        WTF::nullopt, WorkerInspectorProxy::PauseOnWorkerStart::kDontPause,
        ParentFrameTaskRunners::Create());
    return thread;
  }

  void RunBasicTest(WorkerThread* thread) {
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(
            &AudioWorkletGlobalScopeTest::RunBasicTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunSimpleProcessTest(WorkerThread* thread) {
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(
            &AudioWorkletGlobalScopeTest::RunSimpleProcessTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunParsingTest(WorkerThread* thread) {
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(
            &AudioWorkletGlobalScopeTest::RunParsingTestOnWorkletThread,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void RunParsingParameterDescriptorTest(WorkerThread* thread) {
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(&AudioWorkletGlobalScopeTest::
                            RunParsingParameterDescriptorTestOnWorkletThread,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(thread),
                        CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

 private:
  // Returns false when a script evaluation error happens.
  bool EvaluateScriptModule(AudioWorkletGlobalScope* global_scope,
                            const String& source_code) {
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    KURL js_url("https://example.com/worklet.js");
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), source_code, js_url, js_url,
        ScriptFetchOptions(), kSharableCrossOrigin,
        TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value = module.Evaluate(script_state);
    return value.IsEmpty();
  }

  // Test if AudioWorkletGlobalScope and V8 components (ScriptState, Isolate)
  // are properly instantiated. Runs a simple processor registration and check
  // if the class definition is correctly registered, then instantiate an
  // AudioWorkletProcessor instance from the definition.
  void RunBasicTestOnWorkletThread(WorkerThread* thread,
                                   WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = ToAudioWorkletGlobalScope(thread->GlobalScope());

    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);

    v8::Isolate* isolate = script_state->GetIsolate();
    EXPECT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            constructor () { super(); }
            process () {}
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

    AudioWorkletProcessorDefinition* definition =
        global_scope->FindDefinition("testProcessor");
    EXPECT_TRUE(definition);
    EXPECT_EQ(definition->GetName(), "testProcessor");
    EXPECT_TRUE(definition->ConstructorLocal(isolate)->IsFunction());
    EXPECT_TRUE(definition->ProcessLocal(isolate)->IsFunction());
    MessageChannel* channel = MessageChannel::Create(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();

    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("testProcessor",
                                      dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);
    EXPECT_EQ(processor->Name(), "testProcessor");
    v8::Local<v8::Value> processor_value =
        ToV8(processor, script_state->GetContext()->Global(), isolate);
    EXPECT_TRUE(processor_value->IsObject());

    wait_event->Signal();
  }

  // Test if various class definition patterns are parsed correctly.
  void RunParsingTestOnWorkletThread(WorkerThread* thread,
                                     WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = ToAudioWorkletGlobalScope(thread->GlobalScope());

    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);

    ScriptState::Scope scope(script_state);

    {
      // registerProcessor() with a valid class definition should define a
      // processor. Note that these classes will fail at the construction time
      // because they're not valid AudioWorkletProcessor.
      String source_code =
          R"JS(
            var class1 = function () {};
            class1.prototype.process = function () {};
            registerProcessor('class1', class1);

            var class2 = function () {};
            class2.prototype = { process: function () {} };
            registerProcessor('class2', class2);
          )JS";
      ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));
      EXPECT_TRUE(global_scope->FindDefinition("class1"));
      EXPECT_TRUE(global_scope->FindDefinition("class2"));
    }

    {
      // registerProcessor() with an invalid class definition should fail to
      // define a processor.
      String source_code =
          R"JS(
            var class3 = function () {};
            Object.defineProperty(class3, 'prototype', {
                get: function () {
                  return {
                    process: function () {}
                  };
                }
              });
            registerProcessor('class3', class3);
          )JS";
      ASSERT_FALSE(EvaluateScriptModule(global_scope, source_code));
      EXPECT_FALSE(global_scope->FindDefinition("class3"));
    }

    wait_event->Signal();
  }

  // Test if the invocation of process() method in AudioWorkletProcessor and
  // AudioWorkletGlobalScope is performed correctly.
  void RunSimpleProcessTestOnWorkletThread(WorkerThread* thread,
                                           WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = ToAudioWorkletGlobalScope(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            constructor () {
              super();
              this.constant_ = 1;
            }
            process (inputs, outputs) {
              let inputChannel = inputs[0][0];
              let outputChannel = outputs[0][0];
              for (let i = 0; i < outputChannel.length; ++i) {
                outputChannel[i] = inputChannel[i] + this.constant_;
              }
            }
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

    MessageChannel* channel = MessageChannel::Create(thread->GlobalScope());
    MessagePortChannel dummy_port_channel = channel->port2()->Disentangle();
    AudioWorkletProcessor* processor =
        global_scope->CreateProcessor("testProcessor",
                                      dummy_port_channel,
                                      SerializedScriptValue::NullValue());
    EXPECT_TRUE(processor);

    Vector<AudioBus*> input_buses;
    Vector<AudioBus*> output_buses;
    HashMap<String, std::unique_ptr<AudioFloatArray>> param_data_map;
    scoped_refptr<AudioBus> input_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    scoped_refptr<AudioBus> output_bus =
        AudioBus::Create(1, kRenderQuantumFrames);
    AudioChannel* input_channel = input_bus->Channel(0);
    AudioChannel* output_channel = output_bus->Channel(0);

    input_buses.push_back(input_bus.get());
    output_buses.push_back(output_bus.get());

    // Fill |input_channel| with 1 and zero out |output_bus|.
    std::fill(input_channel->MutableData(),
              input_channel->MutableData() + input_channel->length(), 1);
    output_bus->Zero();

    // Then invoke the process() method to perform JS buffer manipulation. The
    // output buffer should contain a constant value of 2.
    processor->Process(&input_buses, &output_buses, &param_data_map);
    for (unsigned i = 0; i < output_channel->length(); ++i) {
      EXPECT_EQ(output_channel->Data()[i], 2);
    }

    wait_event->Signal();
  }

  void RunParsingParameterDescriptorTestOnWorkletThread(
      WorkerThread* thread,
      WaitableEvent* wait_event) {
    EXPECT_TRUE(thread->IsCurrentThread());

    auto* global_scope = ToAudioWorkletGlobalScope(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
          class TestProcessor extends AudioWorkletProcessor {
            static get parameterDescriptors () {
              return [{
                name: 'gain',
                defaultValue: 0.707,
                minValue: 0.0,
                maxValue: 1.0
              }];
            }
            constructor () { super(); }
            process () {}
          }
          registerProcessor('testProcessor', TestProcessor);
        )JS";
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

    AudioWorkletProcessorDefinition* definition =
        global_scope->FindDefinition("testProcessor");
    EXPECT_TRUE(definition);
    EXPECT_EQ(definition->GetName(), "testProcessor");

    const Vector<String> param_names =
        definition->GetAudioParamDescriptorNames();
    EXPECT_EQ(param_names[0], "gain");

    const AudioParamDescriptor* descriptor =
        definition->GetAudioParamDescriptor(param_names[0]);
    EXPECT_EQ(descriptor->defaultValue(), 0.707f);
    EXPECT_EQ(descriptor->minValue(), 0.0f);
    EXPECT_EQ(descriptor->maxValue(), 1.0f);

    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AudioWorkletGlobalScopeTest, Basic) {
  std::unique_ptr<AudioWorkletThread> thread = CreateAudioWorkletThread();
  RunBasicTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, Parsing) {
  std::unique_ptr<AudioWorkletThread> thread = CreateAudioWorkletThread();
  RunParsingTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, BufferProcessing) {
  std::unique_ptr<AudioWorkletThread> thread = CreateAudioWorkletThread();
  RunSimpleProcessTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

TEST_F(AudioWorkletGlobalScopeTest, ParsingParameterDescriptor) {
  std::unique_ptr<AudioWorkletThread> thread = CreateAudioWorkletThread();
  RunParsingParameterDescriptorTest(thread.get());
  thread->Terminate();
  thread->WaitForShutdownForTesting();
}

}  // namespace blink
