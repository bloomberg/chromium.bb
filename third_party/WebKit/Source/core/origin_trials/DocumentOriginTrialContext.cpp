// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/DocumentOriginTrialContext.h"

#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"

namespace blink {

class Document;

DocumentOriginTrialContext::DocumentOriginTrialContext(Document* document)
    : m_parent(document)
{
}

Vector<String> DocumentOriginTrialContext::getTokens()
{
    // When in a document, the tokens are provided in a meta tag
    Vector<String> tokens;
    HTMLHeadElement* head = m_parent->head();
    if (head) {
        for (HTMLMetaElement& metaElement : Traversal<HTMLMetaElement>::childrenOf(*head)) {
            if (equalIgnoringCase(metaElement.httpEquiv(), kTrialHeaderName)) {
                tokens.append(metaElement.content());
            }
        }
    }
    return tokens;
}

DEFINE_TRACE(DocumentOriginTrialContext)
{
    visitor->trace(m_parent);
    OriginTrialContext::trace(visitor);
}

} // namespace blink
