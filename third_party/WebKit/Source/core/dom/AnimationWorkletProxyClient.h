// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletProxyClient_h
#define AnimationWorkletProxyClient_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/workers/WorkerClients.h"

namespace blink {

class WorkletGlobalScope;

class CORE_EXPORT AnimationWorkletProxyClient
    : public Supplement<WorkerClients> {
 public:
  AnimationWorkletProxyClient() = default;

  static AnimationWorkletProxyClient* From(WorkerClients*);
  static const char* SupplementName();

  virtual void SetGlobalScope(WorkletGlobalScope*) = 0;
  virtual void Dispose() = 0;
  DISALLOW_COPY_AND_ASSIGN(AnimationWorkletProxyClient);
};

CORE_EXPORT void ProvideAnimationWorkletProxyClientTo(
    WorkerClients*,
    AnimationWorkletProxyClient*);

}  // namespace blink

#endif  // AnimationWorkletProxyClient_h
