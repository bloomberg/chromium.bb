// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/SerializedScriptValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "bindings/core/v8/serialization/UnpackedSerializedScriptValue.h"
#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/bindings/ToV8.h"

namespace blink {

// On debug builds, Oilpan contains checks that will fail if a persistent handle
// is destroyed on the wrong thread.
// Flaky test on 'WebKit Win x64 Builder (dbg)'. See crbug.com/771169.
#if defined(OS_WIN)
#define MAYBE_SafeDestructionIfSendingThreadKeepsAlive \
  DISABLED_SafeDestructionIfSendingThreadKeepsAlive
#else
#define MAYBE_SafeDestructionIfSendingThreadKeepsAlive \
  SafeDestructionIfSendingThreadKeepsAlive
#endif
TEST(SerializedScriptValueThreadedTest,
     MAYBE_SafeDestructionIfSendingThreadKeepsAlive) {
  V8TestingScope scope;

  // Start a worker.
  WorkerReportingProxy proxy;
  WorkerThreadForTest worker_thread(nullptr, proxy);
  ParentFrameTaskRunners* parent_frame_task_runners =
      ParentFrameTaskRunners::Create(scope.GetFrame());
  worker_thread.StartWithSourceCode(scope.GetDocument().GetSecurityOrigin(),
                                    "/* no worker script */",
                                    parent_frame_task_runners);

  // Create a serialized script value that contains transferred array buffer
  // contents.
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(1, 1);
  Transferables transferables;
  transferables.array_buffers.push_back(array_buffer);
  SerializedScriptValue::SerializeOptions options;
  options.transferables = &transferables;
  RefPtr<SerializedScriptValue> serialized = SerializedScriptValue::Serialize(
      scope.GetIsolate(),
      ToV8(array_buffer, scope.GetContext()->Global(), scope.GetIsolate()),
      options, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(serialized);
  EXPECT_TRUE(array_buffer->IsNeutered());

  // Deserialize the serialized value on the worker.
  // Intentionally keep a reference on this thread while this occurs.
  //
  // Note that the reference is passed strangely to make sure that the main
  // thread keeps its reference, and the bound callback passes its reference
  // into the block, without keeping an additional reference. (Failing to do
  // this results in a data race.)
  WaitableEvent done;
  worker_thread.GetWorkerBackingThread().BackingThread().PostTask(
      FROM_HERE,
      CrossThreadBind(
          [](WorkerThread* worker_thread, WaitableEvent* done,
             RefPtr<SerializedScriptValue> serialized) {
            WorkerOrWorkletScriptController* script =
                worker_thread->GlobalScope()->ScriptController();
            EXPECT_TRUE(script->IsContextInitialized());
            ScriptState::Scope worker_scope(script->GetScriptState());
            SerializedScriptValue::Unpack(serialized)
                ->Deserialize(worker_thread->GetIsolate());

            // Make sure this thread's references are dropped before the main
            // thread continues.
            serialized = nullptr;
            ThreadState::Current()->CollectAllGarbage();
            done->Signal();
          },
          CrossThreadUnretained(&worker_thread), CrossThreadUnretained(&done),
          serialized));
  done.Wait();

  // Now destroy the value on the main thread.
  EXPECT_TRUE(serialized->HasOneRef());
  serialized = nullptr;

  // Finally, shut down the worker thread.
  worker_thread.Terminate();
  worker_thread.WaitForShutdownForTesting();
}

}  // namespace blink
