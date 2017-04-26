// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerFetchContext_h
#define WorkerFetchContext_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/wtf/Forward.h"

namespace blink {

class WorkerClients;
class WebURLLoader;
class WebWorkerFetchContext;
class WorkerGlobalScope;

CORE_EXPORT void ProvideWorkerFetchContextToWorker(
    WorkerClients*,
    std::unique_ptr<WebWorkerFetchContext>);

// The WorkerFetchContext is a FetchContext for workers (dedicated, shared and
// service workers). This class is used only when off-main-thread-fetch is
// enabled, and is still under development.
// TODO(horo): Implement all methods of FetchContext. crbug.com/443374
class WorkerFetchContext final : public FetchContext {
 public:
  static WorkerFetchContext* Create(WorkerGlobalScope&);
  virtual ~WorkerFetchContext();

  // FetchContext implementation:
  // TODO(horo): Implement more methods.
  std::unique_ptr<WebURLLoader> CreateURLLoader() override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  bool IsControlledByServiceWorker() const override;

 private:
  explicit WorkerFetchContext(std::unique_ptr<WebWorkerFetchContext>);

  std::unique_ptr<WebWorkerFetchContext> web_context_;
};

}  // namespace blink

#endif  // WorkerFetchContext_h
