// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletThread_h
#define AnimationWorkletThread_h

#include "modules/ModulesExport.h"
#include "modules/compositorworker/AbstractAnimationWorkletThread.h"
#include <memory>

namespace blink {

class WorkerReportingProxy;

class MODULES_EXPORT AnimationWorkletThread final
    : public AbstractAnimationWorkletThread {
 public:
  static std::unique_ptr<AnimationWorkletThread> Create(
      ThreadableLoadingContext*,
      WorkerReportingProxy&);
  ~AnimationWorkletThread() override;

 protected:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;

 private:
  AnimationWorkletThread(ThreadableLoadingContext*, WorkerReportingProxy&);
};

}  // namespace blink

#endif  // AnimationWorkletThread_h
