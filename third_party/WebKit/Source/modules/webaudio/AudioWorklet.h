// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorklet_h
#define AudioWorklet_h

#include "core/workers/ThreadedWorklet.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ThreadedWorkletMessagingProxy;
class WorkletGlobalScopeProxy;

class MODULES_EXPORT AudioWorklet final : public ThreadedWorklet {
  // Eager finalization is needed to notify parent object destruction of the
  // GC-managed messaging proxy and to initiate worklet termination.
  EAGERLY_FINALIZE();
  WTF_MAKE_NONCOPYABLE(AudioWorklet);

 public:
  static AudioWorklet* Create(LocalFrame*);
  ~AudioWorklet() override;

  void Initialize() final;
  bool IsInitialized() const final;

  WorkletGlobalScopeProxy* GetWorkletGlobalScopeProxy() const final;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AudioWorklet(LocalFrame*);

  Member<ThreadedWorkletMessagingProxy> worklet_messaging_proxy_;
};

}  // namespace blink

#endif  // AudioWorklet_h
