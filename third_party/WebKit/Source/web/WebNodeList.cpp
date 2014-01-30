/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebNodeList.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/html/HTMLCollection.h"
#include "wtf/PassRefPtr.h"

#include "WebNode.h"

using namespace WebCore;

namespace blink {

// FIXME(crbug.com/235008): Remove once chromium has been updated to stop using
// WebCollection as a WebNodeList.
class NodeListWithInternalCollection FINAL : public NodeList {
public:
    explicit NodeListWithInternalCollection(HTMLCollection* collection)
        : m_collection(collection)
    {
    }

    // We have null checks in the following methods because WebNodeList does not have a isNull() method
    // and callers need to be moved to WebNodeCollection and need to handle null using
    // WebNodeCollection::isNull().
    virtual unsigned length() const OVERRIDE { return m_collection ? m_collection->length() : 0; }
    virtual Node* item(unsigned index) const OVERRIDE { return m_collection ? m_collection->item(index) : 0; }
    virtual Node* namedItem(const AtomicString& elementId) const OVERRIDE { return m_collection ? m_collection->namedItem(elementId) : 0; }

private:
    RefPtr<HTMLCollection> m_collection;
};

WebNodeList::WebNodeList(const WebNodeCollection& n)
    : m_private(0)
{
    assign(new NodeListWithInternalCollection(n.m_private));
}

void WebNodeList::reset()
{
    assign(0);
}

void WebNodeList::assign(const WebNodeList& other)
{
    NodeList* p = const_cast<NodeList*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebNodeList::WebNodeList(const PassRefPtr<NodeList>& col)
    : m_private(static_cast<NodeList*>(col.leakRef()))
{
}

void WebNodeList::assign(NodeList* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

unsigned WebNodeList::length() const
{
    return m_private->length();
}

WebNode WebNodeList::item(size_t index) const
{
    return WebNode(m_private->item(index));
}

} // namespace blink
