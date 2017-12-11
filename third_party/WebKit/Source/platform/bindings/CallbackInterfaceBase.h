// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackInterfaceBase_h
#define CallbackInterfaceBase_h

#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

// CallbackInterfaceBase is the common base class of all the callback interface
// classes. Most importantly this class provides a way of type dispatching (e.g.
// overload resolutions, SFINAE technique, etc.) so that it's possible to
// distinguish callback interfaces from anything else. Also it provides a common
// implementation of callback interfaces.
//
// As the signatures of callback interface's operations vary, this class does
// not implement any operation. Subclasses will implement it.
class PLATFORM_EXPORT CallbackInterfaceBase {
 public:
  // Whether the callback interface is a "single operation callback interface"
  // or not.
  // https://heycam.github.io/webidl/#dfn-single-operation-callback-interface
  enum SingleOperationOrNot {
    kNotSingleOperation,
    kSingleOperation,
  };

  virtual ~CallbackInterfaceBase() = default;

  v8::Isolate* GetIsolate() {
    return callback_relevant_script_state_->GetIsolate();
  }
  ScriptState* CallbackRelevantScriptState() {
    return callback_relevant_script_state_.get();
  }

 protected:
  CallbackInterfaceBase(v8::Local<v8::Object> callback_object,
                        SingleOperationOrNot);

  v8::Local<v8::Object> CallbackObject() {
    return callback_object_.NewLocal(GetIsolate());
  }
  // Returns true iff the callback interface is a single operation callback
  // interface and the callback interface type value is callable.
  bool IsCallbackObjectCallable() const { return is_callback_object_callable_; }
  ScriptState* IncumbentScriptState() { return incumbent_script_state_.get(); }

 private:
  // The "callback interface type" value.
  // TODO(yukishiino): Replace ScopedPersistent with TraceWrapperMember.
  ScopedPersistent<v8::Object> callback_object_;
  bool is_callback_object_callable_ = false;
  // The associated Realm of the callback interface type value. Note that the
  // callback interface type value can be different from the function object
  // to be invoked.
  scoped_refptr<ScriptState> callback_relevant_script_state_;
  // The callback context, i.e. the incumbent Realm when an ECMAScript value is
  // converted to an IDL value.
  // https://heycam.github.io/webidl/#dfn-callback-context
  scoped_refptr<ScriptState> incumbent_script_state_;
};

}  // namespace blink

#endif  // CallbackInterfaceBase_h
