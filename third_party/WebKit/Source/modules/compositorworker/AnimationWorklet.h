// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorklet_h
#define AnimationWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;

// Represents the animation worklet on the main thread. All the logic for
// loading a new source module is implemented in its parent class |Worklet|. The
// sole responsibility of this class it to create the appropriate
// |WorkletGlobalScopeProxy| instances that are responsible to proxy a
// corresponding |AnimationWorkletGlobalScope| on the worklet thread.
class MODULES_EXPORT AnimationWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(AnimationWorklet);

 public:
  static AnimationWorklet* Create(LocalFrame*);
  ~AnimationWorklet() override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit AnimationWorklet(LocalFrame*);

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;
};

}  // namespace blink

#endif  // AnimationWorklet_h
