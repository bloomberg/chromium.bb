// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletGlobalScope_h
#define AnimationWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "modules/ModulesExport.h"
#include "modules/compositorworker/Animator.h"
#include "modules/compositorworker/AnimatorDefinition.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ExceptionState;
class WorkerClients;

// Represents the animation worklet global scope and implements all methods that
// the global scope exposes to user script (See
// |AnimationWorkletGlobalScope.idl|). The instances of this class live on the
// worklet thread but have a corresponding proxy on the main thread which is
// accessed by the animation worklet instance. User scripts can register
// animator definitions with the global scope (via |registerAnimator| method).
// The scope keeps a map of these animator definitions and can look them up
// based on their name. The scope also owns a list of active animators that it
// animates.
class MODULES_EXPORT AnimationWorkletGlobalScope
    : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationWorkletGlobalScope* Create(
      const KURL&,
      const String& user_agent,
      RefPtr<SecurityOrigin> document_security_origin,
      v8::Isolate*,
      WorkerThread*,
      WorkerClients*);
  ~AnimationWorkletGlobalScope() override;
  void Trace(blink::Visitor*);
  DECLARE_TRACE_WRAPPERS();
  void Dispose() override;
  bool IsAnimationWorkletGlobalScope() const final { return true; }

  Animator* CreateInstance(const String& name);

  // Invokes the |animate| function of all of its active animators.
  void Mutate();

  // Registers a animator definition with the given name and constructor.
  void registerAnimator(const String& name,
                        const ScriptValue& ctorValue,
                        ExceptionState&);

  AnimatorDefinition* FindDefinitionForTest(const String& name);

 private:
  AnimationWorkletGlobalScope(const KURL&,
                              const String& user_agent,
                              RefPtr<SecurityOrigin> document_security_origin,
                              v8::Isolate*,
                              WorkerThread*,
                              WorkerClients*);

  typedef HeapHashMap<String, TraceWrapperMember<AnimatorDefinition>>
      DefinitionMap;
  DefinitionMap animator_definitions_;

  typedef HeapVector<TraceWrapperMember<Animator>> AnimatorList;
  AnimatorList animators_;
};

DEFINE_TYPE_CASTS(AnimationWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsAnimationWorkletGlobalScope(),
                  context.IsAnimationWorkletGlobalScope());

}  // namespace blink

#endif  // AnimationWorkletGlobalScope_h
