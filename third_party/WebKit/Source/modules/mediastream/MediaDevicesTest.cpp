// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/mediastream/MediaDevices.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PromiseObserver {
 public:
  PromiseObserver(ScriptState* script_state, ScriptPromise promise)
      : is_rejected_(false), is_fulfilled_(false) {
    v8::Local<v8::Function> on_fulfilled = MyScriptFunction::CreateFunction(
        script_state, &is_fulfilled_, &saved_arg_);
    v8::Local<v8::Function> on_rejected = MyScriptFunction::CreateFunction(
        script_state, &is_rejected_, &saved_arg_);
    promise.Then(on_fulfilled, on_rejected);
  }

  bool isDecided() { return is_rejected_ || is_fulfilled_; }

  bool isFulfilled() { return is_fulfilled_; }
  bool isRejected() { return is_rejected_; }
  ScriptValue argument() { return saved_arg_; }

 private:
  class MyScriptFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                  bool* flag_to_set,
                                                  ScriptValue* arg_to_set) {
      MyScriptFunction* self =
          new MyScriptFunction(script_state, flag_to_set, arg_to_set);
      return self->BindToV8Function();
    }

   private:
    MyScriptFunction(ScriptState* script_state,
                     bool* flag_to_set,
                     ScriptValue* arg_to_set)
        : ScriptFunction(script_state),
          flag_to_set_(flag_to_set),
          arg_to_set_(arg_to_set) {}
    ScriptValue Call(ScriptValue arg) {
      *flag_to_set_ = true;
      *arg_to_set_ = arg;
      return arg;
    }
    bool* flag_to_set_;
    ScriptValue* arg_to_set_;
  };

  bool is_rejected_;
  bool is_fulfilled_;
  ScriptValue saved_arg_;
};

TEST(MediaDevicesTest, GetUserMediaCanBeCalled) {
  V8TestingScope scope;
  auto devices = MediaDevices::Create(scope.GetExecutionContext());
  MediaStreamConstraints constraints;
  ScriptPromise promise = devices->getUserMedia(
      scope.GetScriptState(), constraints, scope.GetExceptionState());
  ASSERT_FALSE(promise.IsEmpty());
  PromiseObserver promise_observer(scope.GetScriptState(), promise);
  EXPECT_FALSE(promise_observer.isDecided());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(promise_observer.isDecided());
  // In the default test environment, we expect a DOM rejection because
  // the script state's execution context's document's frame doesn't
  // have an UserMediaController.
  EXPECT_TRUE(promise_observer.isRejected());
  // TODO(hta): Check that the correct error ("not supported") is returned.
  EXPECT_FALSE(promise_observer.argument().IsNull());
  // This log statement is included as a demonstration of how to get the string
  // value of the argument.
  VLOG(1) << "Argument is"
          << ToCoreString(promise_observer.argument()
                              .V8Value()
                              ->ToString(scope.GetContext())
                              .ToLocalChecked());
}

}  // namespace blink
