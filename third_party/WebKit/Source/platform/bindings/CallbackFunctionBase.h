// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackFunctionBase_h
#define CallbackFunctionBase_h

#include "platform/bindings/ScriptState.h"
#include "platform/bindings/TraceWrapperBase.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"

namespace blink {

// CallbackFunctionBase is the common base class of all the callback function
// classes. Most importantly this class provides a way of type dispatching (e.g.
// overload resolutions, SFINAE technique, etc.) so that it's possible to
// distinguish callback functions from anything else. Also it provides a common
// implementation of callback functions.
//
// As the signatures of callback functions vary, this class does not implement
// |Invoke| member function that performs "invoke" steps. Subclasses will
// implement it.
class PLATFORM_EXPORT CallbackFunctionBase
    : public GarbageCollectedFinalized<CallbackFunctionBase>,
      public TraceWrapperBase {
 public:
  virtual ~CallbackFunctionBase() = default;

  virtual void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

  v8::Isolate* GetIsolate() {
    return callback_relevant_script_state_->GetIsolate();
  }

 protected:
  explicit CallbackFunctionBase(v8::Local<v8::Function>);

  v8::Local<v8::Function> CallbackFunction() {
    return callback_function_.NewLocal(GetIsolate());
  }
  ScriptState* CallbackRelevantScriptState() {
    return callback_relevant_script_state_.get();
  }
  ScriptState* IncumbentScriptState() { return incumbent_script_state_.get(); }

 private:
  // The "callback function type" value.
  TraceWrapperV8Reference<v8::Function> callback_function_;
  // The associated Realm of the callback function type value.
  scoped_refptr<ScriptState> callback_relevant_script_state_;
  // The callback context, i.e. the incumbent Realm when an ECMAScript value is
  // converted to an IDL value.
  // https://heycam.github.io/webidl/#dfn-callback-context
  scoped_refptr<ScriptState> incumbent_script_state_;
};

}  // namespace blink

#endif  // CallbackFunctionBase_h
