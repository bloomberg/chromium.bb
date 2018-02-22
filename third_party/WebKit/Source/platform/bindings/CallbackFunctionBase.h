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

template <typename V8CallbackFunction>
class V8PersistentCallbackFunction;

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

  v8::Isolate* GetIsolate() const {
    return callback_relevant_script_state_->GetIsolate();
  }
  ScriptState* CallbackRelevantScriptState() {
    return callback_relevant_script_state_.get();
  }

 protected:
  explicit CallbackFunctionBase(v8::Local<v8::Function>);
  explicit CallbackFunctionBase(const CallbackFunctionBase& other);

  v8::Local<v8::Function> CallbackFunction() const {
    return callback_function_.NewLocal(GetIsolate());
  }
  ScriptState* IncumbentScriptState() { return incumbent_script_state_.get(); }

 private:
  template <typename V8CallbackFunction>
  friend class V8PersistentCallbackFunction;

  // The "callback function type" value.
  TraceWrapperV8Reference<v8::Function> callback_function_;
  // The associated Realm of the callback function type value.
  scoped_refptr<ScriptState> callback_relevant_script_state_;
  // The callback context, i.e. the incumbent Realm when an ECMAScript value is
  // converted to an IDL value.
  // https://heycam.github.io/webidl/#dfn-callback-context
  scoped_refptr<ScriptState> incumbent_script_state_;
};

// V8PersistentCallbackFunction retains the underlying v8::Function of a
// V8CallbackFunction, which must be a subclass of CallbackFunctionBase, without
// wrapper-tracing. This class is necessary and useful where wrapper-tracing is
// not suitable. Remember that, as a nature of v8::Persistent, abuse of
// V8PersistentCallbackFunction would result in memory leak, so the use of
// V8PersistentCallbackFunction should be limited to those which are guaranteed
// to release the persistents in a finite time period.
template <typename V8CallbackFunction>
class V8PersistentCallbackFunction final : public V8CallbackFunction {
 public:
  static V8PersistentCallbackFunction* Create(
      V8CallbackFunction* callback_function) {
    return callback_function
               ? new V8PersistentCallbackFunction(callback_function)
               : nullptr;
  }

  ~V8PersistentCallbackFunction() override { v8_function_.Reset(); }

 private:
  explicit V8PersistentCallbackFunction(V8CallbackFunction* callback_function)
      : V8CallbackFunction(*callback_function) {
    v8_function_.Reset(this->GetIsolate(), this->callback_function_.Get());
  }

  v8::Persistent<v8::Function> v8_function_;
};

// TODO(yukishiino): Remove WrapPersistentCallbackFunction.
// blink::WrapPersistent + V8PersistentCallbackFunction is preferred.
template <typename T>
auto WrapPersistentCallbackFunction(T* value) {
  return WrapPersistent(V8PersistentCallbackFunction<T>::Create(value));
}

}  // namespace blink

#endif  // CallbackFunctionBase_h
