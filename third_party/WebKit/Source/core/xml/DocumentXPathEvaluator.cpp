/*
 * Copyright (C) 2013 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/xml/DocumentXPathEvaluator.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/xml/XPathEvaluator.h"
#include "core/xml/XPathExpression.h"
#include "core/xml/XPathNSResolver.h"
#include "core/xml/XPathResult.h"

namespace WebCore {

DocumentXPathEvaluator::DocumentXPathEvaluator()
{
}

DocumentXPathEvaluator::~DocumentXPathEvaluator()
{
}

DocumentXPathEvaluator* DocumentXPathEvaluator::from(ScriptExecutionContext* context)
{
    DocumentXPathEvaluator* cache = static_cast<DocumentXPathEvaluator*>(Supplement<ScriptExecutionContext>::from(context, supplementName()));
    if (!cache) {
        cache = new DocumentXPathEvaluator();
        Supplement<ScriptExecutionContext>::provideTo(context, supplementName(), adoptPtr(cache));
    }
    return cache;
}

PassRefPtr<XPathExpression> DocumentXPathEvaluator::createExpression(ScriptExecutionContext* context,
    const String& expression, XPathNSResolver* resolver, ExceptionState& es)
{
    DocumentXPathEvaluator* suplement = from(context);
    if (!suplement->m_xpathEvaluator)
        suplement->m_xpathEvaluator = XPathEvaluator::create();
    return suplement->m_xpathEvaluator->createExpression(expression, resolver, es);
}

PassRefPtr<XPathNSResolver> DocumentXPathEvaluator::createNSResolver(ScriptExecutionContext* context, Node* nodeResolver)
{
    DocumentXPathEvaluator* suplement = from(context);
    if (!suplement->m_xpathEvaluator)
        suplement->m_xpathEvaluator = XPathEvaluator::create();
    return suplement->m_xpathEvaluator->createNSResolver(nodeResolver);
}

PassRefPtr<XPathResult> DocumentXPathEvaluator::evaluate(ScriptExecutionContext* context, const String& expression,
    Node* contextNode, XPathNSResolver* resolver, unsigned short type,
    XPathResult* result, ExceptionState& es)
{
    DocumentXPathEvaluator* suplement = from(context);
    if (!suplement->m_xpathEvaluator)
        suplement->m_xpathEvaluator = XPathEvaluator::create();
    return suplement->m_xpathEvaluator->evaluate(expression, contextNode, resolver, type, result, es);
}

} // namespace WebCore
