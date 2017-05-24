// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletGlobalScope_h
#define AnimationWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "modules/compositorworker/Animator.h"
#include "modules/compositorworker/AnimatorDefinition.h"

namespace blink {

class ExceptionState;

class AnimationWorkletGlobalScope : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationWorkletGlobalScope* Create(const KURL&,
                                             const String& user_agent,
                                             PassRefPtr<SecurityOrigin>,
                                             v8::Isolate*,
                                             WorkerThread*);
  ~AnimationWorkletGlobalScope() override;
  DECLARE_TRACE();

  void Dispose() final;

  void registerAnimator(const String& name,
                        const ScriptValue& ctorValue,
                        ExceptionState&);

  Animator* CreateInstance(const String& name);

 private:
  AnimationWorkletGlobalScope(const KURL&,
                              const String& user_agent,
                              PassRefPtr<SecurityOrigin>,
                              v8::Isolate*,
                              WorkerThread*);

  typedef HeapHashMap<String, Member<AnimatorDefinition>> DefinitionMap;
  DefinitionMap m_animatorDefinitions;

  typedef HeapVector<Member<Animator>> AnimatorList;
  AnimatorList m_animators;
};

}  // namespace blink

#endif  // AnimationWorkletGlobalScope_h
