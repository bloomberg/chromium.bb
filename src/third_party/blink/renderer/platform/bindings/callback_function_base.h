// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_

#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class V8PersistentCallbackFunctionBase;

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
      public NameClient {
 public:
  virtual ~CallbackFunctionBase() = default;

  virtual void Trace(blink::Visitor* visitor);

  v8::Local<v8::Object> CallbackObject() {
    return callback_function_.NewLocal(GetIsolate());
  }

  v8::Isolate* GetIsolate() const {
    return incumbent_script_state_->GetIsolate();
  }

  // Returns the ScriptState of the relevant realm of the callback object.
  //
  // NOTE: This function must be used only when it's pretty sure that the
  // callcack object is the same origin-domain. Otherwise,
  // |CallbackRelevantScriptStateOrReportError| or
  // |CallbackRelevantScriptStateOrThrowException| must be used instead.
  ScriptState* CallbackRelevantScriptState() {
    DCHECK(callback_relevant_script_state_);
    return callback_relevant_script_state_;
  }

  // Returns the ScriptState of the relevant realm of the callback object iff
  // the callback is the same origin-domain. Otherwise, reports an error and
  // returns nullptr.
  ScriptState* CallbackRelevantScriptStateOrReportError(const char* interface,
                                                        const char* operation);

  // Returns the ScriptState of the relevant realm of the callback object iff
  // the callback is the same origin-domain. Otherwise, throws an exception and
  // returns nullptr.
  ScriptState* CallbackRelevantScriptStateOrThrowException(
      const char* interface,
      const char* operation);

  DOMWrapperWorld& GetWorld() const { return incumbent_script_state_->World(); }

  // Returns true if the ES function has a [[Construct]] internal method.
  bool IsConstructor() const { return CallbackFunction()->IsConstructor(); }

  // Makes the underlying V8 function collectable by V8 Scavenger GC.  Do not
  // use this function unless you really need a hacky performance optimization.
  // The V8 function is collectable by V8 Full GC whenever this instance is no
  // longer referenced, so there is no need to call this function unless you
  // really need V8 *Scavenger* GC to collect the V8 function before V8 Full GC
  // runs.
  void DisposeV8FunctionImmediatelyToReduceMemoryFootprint() {
    callback_function_.Clear();
  }

 protected:
  explicit CallbackFunctionBase(v8::Local<v8::Object>);

  v8::Local<v8::Function> CallbackFunction() const {
    return callback_function_.NewLocal(GetIsolate()).As<v8::Function>();
  }

  ScriptState* IncumbentScriptState() { return incumbent_script_state_; }

 private:
  // The "callback function type" value.
  // Use v8::Object instead of v8::Function in order to handle
  // [TreatNonObjectAsNull].
  TraceWrapperV8Reference<v8::Object> callback_function_;
  // The associated Realm of the callback function type value iff it's the same
  // origin-domain. Otherwise, nullptr.
  Member<ScriptState> callback_relevant_script_state_;
  // The callback context, i.e. the incumbent Realm when an ECMAScript value is
  // converted to an IDL value.
  // https://heycam.github.io/webidl/#dfn-callback-context
  Member<ScriptState> incumbent_script_state_;

  friend class V8PersistentCallbackFunctionBase;
};

// V8PersistentCallbackFunctionBase retains the underlying v8::Function of a
// CallbackFunctionBase without wrapper-tracing. This class is necessary and
// useful where wrapper-tracing is not suitable. Remember that, as a nature of
// v8::Persistent, abuse of V8PersistentCallbackFunctionBase would result in
// memory leak, so the use of V8PersistentCallbackFunctionBase should be limited
// to those which are guaranteed to release the persistents in a finite time
// period.
class PLATFORM_EXPORT V8PersistentCallbackFunctionBase
    : public GarbageCollectedFinalized<V8PersistentCallbackFunctionBase> {
 public:
  virtual ~V8PersistentCallbackFunctionBase() { v8_function_.Reset(); }

  virtual void Trace(blink::Visitor*);

 protected:
  explicit V8PersistentCallbackFunctionBase(CallbackFunctionBase*);

  template <typename V8CallbackFunction>
  V8CallbackFunction* As() {
    static_assert(
        std::is_base_of<CallbackFunctionBase, V8CallbackFunction>::value,
        "V8CallbackFunction must be a subclass of CallbackFunctionBase.");
    return static_cast<V8CallbackFunction*>(callback_function_.Get());
  }

 private:
  Member<CallbackFunctionBase> callback_function_;
  // Use v8::Object instead of v8::Function in order to handle
  // [TreatNonObjectAsNull].
  v8::Persistent<v8::Object> v8_function_;
};

// V8PersistentCallbackFunction<V8CallbackFunction> is a counter-part of
// V8CallbackFunction. While V8CallbackFunction uses wrapper-tracing,
// V8PersistentCallbackFunction<V8CallbackFunction> uses v8::Persistent to make
// the underlying v8::Function alive.
//
// Since the signature of |Invoke| varies depending on the IDL definition,
// the class definition is specialized and generated by the bindings code
// generator.
template <typename V8CallbackFunction>
class V8PersistentCallbackFunction;

// Converts the wrapper-tracing version of a callback function to the
// v8::Persistent version of it.
template <typename V8CallbackFunction>
inline V8PersistentCallbackFunction<V8CallbackFunction>*
ToV8PersistentCallbackFunction(V8CallbackFunction* callback_function) {
  static_assert(
      std::is_base_of<CallbackFunctionBase, V8CallbackFunction>::value,
      "V8CallbackFunction must be a subclass of CallbackFunctionBase.");
  return callback_function
             ? MakeGarbageCollected<
                   V8PersistentCallbackFunction<V8CallbackFunction>>(
                   callback_function)
             : nullptr;
}

// CallbackFunctionBase is designed to be used with wrapper-tracing. As
// blink::Persistent does not perform wrapper-tracing, use of |WrapPersistent|
// for callback functions is likely (if not always) misuse. Thus, this code
// prohibits such a use case. The call sites should explicitly use
// WrapPersistent(V8PersistentCallbackFunction<T>*).
Persistent<CallbackFunctionBase> WrapPersistent(CallbackFunctionBase*) = delete;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_FUNCTION_BASE_H_
