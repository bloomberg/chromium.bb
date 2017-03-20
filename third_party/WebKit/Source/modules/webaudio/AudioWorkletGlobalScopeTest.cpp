// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletProcessorDefinition.h"
#include "modules/webaudio/AudioWorkletThread.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

static const size_t kRenderQuantumFrames = 128;

// This is a typical sample rate.
static const float kTestingSampleRate = 44100;

// A null WorkerReportingProxy, supplied when creating AudioWorkletThreads.
class TestAudioWorkletReportingProxy : public WorkerReportingProxy {
 public:
  static std::unique_ptr<TestAudioWorkletReportingProxy> create() {
    return WTF::wrapUnique(new TestAudioWorkletReportingProxy());
  }

  // (Empty) WorkerReportingProxy implementation:
  void countFeature(UseCounter::Feature) override {}
  void countDeprecation(UseCounter::Feature) override {}
  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override {}
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override {}
  void postMessageToPageInspector(const String&) override {}
  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override {}
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override {}

 private:
  TestAudioWorkletReportingProxy() {}
};

}  // namespace

class AudioWorkletGlobalScopeTest : public ::testing::Test {
 public:
  void SetUp() override {
    AudioWorkletThread::createSharedBackingThreadForTest();
    m_reportingProxy = TestAudioWorkletReportingProxy::create();
    m_securityOrigin =
        SecurityOrigin::create(KURL(ParsedURLString, "http://fake.url/"));
  }

  void TearDown() override { AudioWorkletThread::clearSharedBackingThread(); }

  std::unique_ptr<AudioWorkletThread> createAudioWorkletThread() {
    std::unique_ptr<AudioWorkletThread> thread =
        AudioWorkletThread::create(nullptr, *m_reportingProxy);
    thread->start(
        WorkerThreadStartupData::create(
            KURL(ParsedURLString, "http://fake.url/"), "fake user agent", "",
            nullptr, DontPauseWorkerGlobalScopeOnStart, nullptr, "",
            m_securityOrigin.get(), nullptr, WebAddressSpaceLocal, nullptr,
            nullptr, WorkerV8Settings::Default()),
        ParentFrameTaskRunners::create(nullptr));
    return thread;
  }

  void runBasicTest(WorkerThread* thread) {
    WaitableEvent waitableEvent;
    thread->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(
            &AudioWorkletGlobalScopeTest::runBasicTestOnWorkletThread,
            crossThreadUnretained(this), crossThreadUnretained(thread),
            crossThreadUnretained(&waitableEvent)));
    waitableEvent.wait();
  }

  void runSimpleProcessTest(WorkerThread* thread) {
    WaitableEvent waitableEvent;
    thread->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(
            &AudioWorkletGlobalScopeTest::runSimpleProcessTestOnWorkletThread,
            crossThreadUnretained(this), crossThreadUnretained(thread),
            crossThreadUnretained(&waitableEvent)));
    waitableEvent.wait();
  }

  void runParsingTest(WorkerThread* thread) {
    WaitableEvent waitableEvent;
    thread->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(
            &AudioWorkletGlobalScopeTest::runParsingTestOnWorkletThread,
            crossThreadUnretained(this), crossThreadUnretained(thread),
            crossThreadUnretained(&waitableEvent)));
    waitableEvent.wait();
  }

 private:
  // Test if AudioWorkletGlobalScope and V8 components (ScriptState, Isolate)
  // are properly instantiated. Runs a simple processor registration and check
  // if the class definition is correctly registered, then instantiate an
  // AudioWorkletProcessor instance from the definition.
  void runBasicTestOnWorkletThread(WorkerThread* thread,
                                   WaitableEvent* waitEvent) {
    EXPECT_TRUE(thread->isCurrentThread());

    AudioWorkletGlobalScope* globalScope =
        static_cast<AudioWorkletGlobalScope*>(thread->globalScope());
    EXPECT_TRUE(globalScope);
    EXPECT_TRUE(globalScope->isAudioWorkletGlobalScope());

    ScriptState* scriptState =
        globalScope->scriptController()->getScriptState();
    EXPECT_TRUE(scriptState);

    v8::Isolate* isolate = scriptState->isolate();
    EXPECT_TRUE(isolate);

    ScriptState::Scope scope(scriptState);

    globalScope->scriptController()->evaluate(ScriptSourceCode(
        R"JS(
          registerProcessor('testProcessor', class {
              constructor () {}
              process () {}
            });
          )JS"));

    AudioWorkletProcessorDefinition* definition =
        globalScope->findDefinition("testProcessor");
    EXPECT_TRUE(definition);
    EXPECT_EQ(definition->name(), "testProcessor");
    EXPECT_TRUE(definition->constructorLocal(isolate)->IsFunction());
    EXPECT_TRUE(definition->processLocal(isolate)->IsFunction());

    AudioWorkletProcessor* processor =
        globalScope->createInstance("testProcessor");
    EXPECT_TRUE(processor);
    EXPECT_EQ(processor->name(), "testProcessor");
    EXPECT_TRUE(processor->instanceLocal(isolate)->IsObject());

    waitEvent->signal();
  }

  // Test if various class definition patterns are parsed correctly.
  void runParsingTestOnWorkletThread(WorkerThread* thread,
                                     WaitableEvent* waitEvent) {
    EXPECT_TRUE(thread->isCurrentThread());

    AudioWorkletGlobalScope* globalScope =
        static_cast<AudioWorkletGlobalScope*>(thread->globalScope());
    ScriptState* scriptState =
        globalScope->scriptController()->getScriptState();

    ScriptState::Scope scope(scriptState);

    globalScope->scriptController()->evaluate(ScriptSourceCode(
        R"JS(
          var class1 = function () {};
          class1.prototype.process = function () {};
          registerProcessor('class1', class1);

          var class2 = function () {};
          class2.prototype = { process: function () {} };
          registerProcessor('class2', class2);

          var class3 = function () {};
          Object.defineProperty(class3, 'prototype', {
              get: function () {
                return {
                  process: function () {}
                };
              }
            });
          registerProcessor('class3', class3);
        )JS"));

    EXPECT_TRUE(globalScope->findDefinition("class1"));
    EXPECT_TRUE(globalScope->findDefinition("class2"));
    EXPECT_FALSE(globalScope->findDefinition("class3"));

    waitEvent->signal();
  }

  // Test if the invocation of process() method in AudioWorkletProcessor and
  // AudioWorkletGlobalScope is performed correctly.
  void runSimpleProcessTestOnWorkletThread(WorkerThread* thread,
                                           WaitableEvent* waitEvent) {
    EXPECT_TRUE(thread->isCurrentThread());

    AudioWorkletGlobalScope* globalScope =
        static_cast<AudioWorkletGlobalScope*>(thread->globalScope());
    ScriptState* scriptState =
        globalScope->scriptController()->getScriptState();

    ScriptState::Scope scope(scriptState);

    globalScope->scriptController()->evaluate(ScriptSourceCode(
        R"JS(
          registerProcessor('testProcessor', class {
              constructor () {
                this.constant = 1;
              }
              process (input, output) {
                let inputChannelData = input.getChannelData(0);
                let outputChannelData = output.getChannelData(0);
                for (let i = 0; i < input.length; ++i) {
                  outputChannelData[i] = inputChannelData[i] + this.constant;
                }
              }
            }
          )
        )JS"));

    AudioWorkletProcessor* processor =
        globalScope->createInstance("testProcessor");
    EXPECT_TRUE(processor);

    AudioBuffer* inputBuffer =
        AudioBuffer::create(1, kRenderQuantumFrames, kTestingSampleRate);
    AudioBuffer* outputBuffer =
        AudioBuffer::create(1, kRenderQuantumFrames, kTestingSampleRate);
    DOMFloat32Array* inputChannelData = inputBuffer->getChannelData(0);
    float* inputArrayData = inputChannelData->data();
    EXPECT_TRUE(inputArrayData);
    DOMFloat32Array* outputChannelData = outputBuffer->getChannelData(0);
    float* outputArrayData = outputChannelData->data();
    EXPECT_TRUE(outputArrayData);

    // Fill |inputBuffer| with 1 and zero out |outputBuffer|.
    std::fill(inputArrayData, inputArrayData + inputBuffer->length(), 1);
    outputBuffer->zero();

    // Then invoke the process() method to perform JS buffer manipulation. The
    // output buffer should contain a constant value of 2.
    processor->process(inputBuffer, outputBuffer);
    for (unsigned i = 0; i < outputBuffer->length(); ++i) {
      EXPECT_EQ(outputArrayData[i], 2);
    }

    waitEvent->signal();
  }

  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<WorkerReportingProxy> m_reportingProxy;
};

TEST_F(AudioWorkletGlobalScopeTest, Basic) {
  std::unique_ptr<AudioWorkletThread> thread = createAudioWorkletThread();
  runBasicTest(thread.get());
  thread->terminateAndWait();
}

TEST_F(AudioWorkletGlobalScopeTest, Parsing) {
  std::unique_ptr<AudioWorkletThread> thread = createAudioWorkletThread();
  runParsingTest(thread.get());
  thread->terminateAndWait();
}

TEST_F(AudioWorkletGlobalScopeTest, BufferProcessing) {
  std::unique_ptr<AudioWorkletThread> thread = createAudioWorkletThread();
  runSimpleProcessTest(thread.get());
  thread->terminateAndWait();
}

}  // namespace blink
