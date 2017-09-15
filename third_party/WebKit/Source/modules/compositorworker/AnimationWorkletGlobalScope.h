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

class MODULES_EXPORT AnimationWorkletGlobalScope
    : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationWorkletGlobalScope* Create(const KURL&,
                                             const String& user_agent,
                                             RefPtr<SecurityOrigin>,
                                             v8::Isolate*,
                                             WorkerThread*,
                                             WorkerClients*);
  ~AnimationWorkletGlobalScope() override;
  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();
  void Dispose() override;
  bool IsAnimationWorkletGlobalScope() const final { return true; }

  Animator* CreateInstance(const String& name);
  void Mutate();

  void registerAnimator(const String& name,
                        const ScriptValue& ctorValue,
                        ExceptionState&);

  AnimatorDefinition* FindDefinitionForTest(const String& name);

 private:
  AnimationWorkletGlobalScope(const KURL&,
                              const String& user_agent,
                              RefPtr<SecurityOrigin>,
                              v8::Isolate*,
                              WorkerThread*,
                              WorkerClients*);

  typedef HeapHashMap<String, TraceWrapperMember<AnimatorDefinition>>
      DefinitionMap;
  DefinitionMap animator_definitions_;

  typedef HeapVector<TraceWrapperMember<Animator>> AnimatorList;
  AnimatorList animators_;
};

}  // namespace blink

#endif  // AnimationWorkletGlobalScope_h
