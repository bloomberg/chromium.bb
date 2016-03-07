// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentOriginTrialContext_h
#define DocumentOriginTrialContext_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include <utility>

namespace blink {

class WebApiKeyValidator;

// DocumentOriginTrialContext is a specialization of OriginTrialContext for
// the Document execution context. It enables and disables feature trials based
// on the tokens found in <meta> tags in the document header.
class CORE_EXPORT DocumentOriginTrialContext : public OriginTrialContext {
public:
    explicit DocumentOriginTrialContext(Document*);
    ~DocumentOriginTrialContext() override = default;

    ExecutionContext* executionContext() override { return m_parent; }
    Vector<String> getTokens() override;

    DECLARE_VIRTUAL_TRACE();

protected:
    friend class DocumentOriginTrialContextTest;

private:
    RawPtrWillBeMember<Document> m_parent;
};

} // namespace blink

#endif // DocumentOriginTrialContext_h
