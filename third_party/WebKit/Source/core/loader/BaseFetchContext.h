// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BaseFetchContext_h
#define BaseFetchContext_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/ResourceRequest.h"

namespace blink {

class CORE_EXPORT BaseFetchContext : public FetchContext {
 public:
  explicit BaseFetchContext(ExecutionContext*);
  ~BaseFetchContext() override { execution_context_ = nullptr; }

  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  SecurityOrigin* GetSecurityOrigin() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  void PrintAccessDeniedMessage(const KURL&) const;
  void AddCSPHeaderIfNecessary(Resource::Type, ResourceRequest&);

  // FIXME: Oilpan: Ideally this should just be a traced Member but that will
  // currently leak because ComputedStyle and its data are not on the heap.
  // See crbug.com/383860 for details.
  WeakMember<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // BaseFetchContext_h
