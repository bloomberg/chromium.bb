// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorker_h
#define CompositorWorker_h

#include "core/workers/InProcessWorkerBase.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class InProcessWorkerMessagingProxy;

class MODULES_EXPORT CompositorWorker final : public InProcessWorkerBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(CompositorWorker);

 public:
  static CompositorWorker* Create(ExecutionContext*,
                                  const String& url,
                                  ExceptionState&);
  ~CompositorWorker() override;

  const AtomicString& InterfaceName() const override;
  InProcessWorkerMessagingProxy* CreateInProcessWorkerMessagingProxy(
      ExecutionContext*) override;

 private:
  explicit CompositorWorker(ExecutionContext*);
};

}  // namespace blink

#endif  // CompositorWorker_h
