// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worker_h
#define Worker_h

#include "core/workers/InProcessWorkerBase.h"

#include "core/workers/WorkerClients.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class InProcessWorkerMessagingProxy;

class CORE_EXPORT Worker final : public InProcessWorkerBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worker);

 public:
  static Worker* Create(ExecutionContext*, const String& url, ExceptionState&);
  ~Worker() override;

 protected:
  explicit Worker(ExecutionContext*);

  InProcessWorkerMessagingProxy* CreateInProcessWorkerMessagingProxy(
      ExecutionContext*) override;
  const AtomicString& InterfaceName() const override;
};

}  // namespace blink

#endif  // Worker_h
