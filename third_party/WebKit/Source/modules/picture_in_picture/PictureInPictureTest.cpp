// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/testing/PageTestBase.h"
#include "modules/picture_in_picture/HTMLVideoElementPictureInPicture.h"
#include "modules/picture_in_picture/PictureInPictureController.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

const char kNotSupportedString[] =
    "NotSupportedError: Picture-in-Picture is not available.";

class PictureInPictureTest : public PageTestBase {
 protected:
  void SetUp() final { PageTestBase::SetUp(); }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }
  v8::Isolate* GetIsolate() { return GetScriptState()->GetIsolate(); }
  v8::Local<v8::Context> GetContext() { return GetScriptState()->GetContext(); }

  // Convenience methods for testing the returned promises.
  ScriptValue GetRejectValue(ScriptPromise& promise) {
    ScriptValue on_reject;
    promise.Then(UnreachableFunction::Create(GetScriptState()),
                 TestFunction::Create(GetScriptState(), &on_reject));
    v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
    return on_reject;
  }

  std::string GetRejectString(ScriptPromise& promise) {
    ScriptValue on_reject = GetRejectValue(promise);
    return ToCoreString(
               on_reject.V8Value()->ToString(GetContext()).ToLocalChecked())
        .Ascii()
        .data();
  }

 private:
  // A ScriptFunction that creates a test failure if it is ever called.
  class UnreachableFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state) {
      UnreachableFunction* self = new UnreachableFunction(script_state);
      return self->BindToV8Function();
    }

    ScriptValue Call(ScriptValue value) override {
      ADD_FAILURE() << "Unexpected call to a null ScriptFunction.";
      return value;
    }

   private:
    UnreachableFunction(ScriptState* script_state)
        : ScriptFunction(script_state) {}
  };

  // A ScriptFunction that saves its parameter; used by tests to assert on
  // correct values being passed.
  class TestFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state,
                                          ScriptValue* out_value) {
      TestFunction* self = new TestFunction(script_state, out_value);
      return self->BindToV8Function();
    }

    ScriptValue Call(ScriptValue value) override {
      DCHECK(!value.IsEmpty());
      *value_ = value;
      return value;
    }

   private:
    TestFunction(ScriptState* script_state, ScriptValue* out_value)
        : ScriptFunction(script_state), value_(out_value) {}

    ScriptValue* value_;
  };
};

TEST_F(PictureInPictureTest,
       RequestPictureInPictureRejectsWhenPictureInPictureEnabledIsFalse) {
  Persistent<PictureInPictureController> controller =
      PictureInPictureController::Ensure(GetDocument());
  ScriptState::Scope scope(GetScriptState());
  HTMLVideoElement& video =
      static_cast<HTMLVideoElement&>(*HTMLVideoElement::Create(GetDocument()));

  controller->SetPictureInPictureEnabledForTesting(false);
  ScriptPromise promise =
      HTMLVideoElementPictureInPicture::requestPictureInPicture(
          GetScriptState(), video);

  EXPECT_EQ(kNotSupportedString, GetRejectString(promise));
}

TEST_F(PictureInPictureTest,
       PictureInPictureEnabledReturnsFalseWhenPictureInPictureEnabledIsFalse) {
  Persistent<PictureInPictureController> controller =
      PictureInPictureController::Ensure(GetDocument());

  controller->SetPictureInPictureEnabledForTesting(false);

  EXPECT_FALSE(controller->PictureInPictureEnabled());
}

}  // namespace blink
