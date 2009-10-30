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
#include "AccessibilityObject.h"
#include "WebAccessibilityObject.h"
#include "WebString.h"

using namespace WebCore;

namespace WebKit {

class WebAccessibilityObjectPrivate : public WebCore::AccessibilityObject {
};

void WebAccessibilityObject::reset()
{
    assign(0);
}

void WebAccessibilityObject::assign(const WebKit::WebAccessibilityObject& other)
{
    WebAccessibilityObjectPrivate* p = const_cast<WebAccessibilityObjectPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebString WebAccessibilityObject::accessibilityDescription() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->accessibilityDescription();
}

WebAccessibilityObject WebAccessibilityObject::childAt(unsigned index) const
{
    if (!m_private)
        return WebAccessibilityObject();

    m_private->updateBackingStore();
    if (m_private->children().size() <= index)
        return WebAccessibilityObject();

    return WebAccessibilityObject(m_private->children()[index]);
}

unsigned WebAccessibilityObject::childCount() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->children().size();
}

bool WebAccessibilityObject::isEnabled() const
{
    if (!m_private)
        return 0;

    m_private->updateBackingStore();
    return m_private->isEnabled();
}

WebAccessibilityRole WebAccessibilityObject::roleValue() const
{
    m_private->updateBackingStore();
    return static_cast<WebAccessibilityRole>(m_private->roleValue());
}

WebString WebAccessibilityObject::title() const
{
    if (!m_private)
        return WebString();

    m_private->updateBackingStore();
    return m_private->title();
}

WebAccessibilityObject::WebAccessibilityObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
    : m_private(static_cast<WebAccessibilityObjectPrivate*>(object.releaseRef()))
{
}

WebAccessibilityObject& WebAccessibilityObject::operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
{
    assign(static_cast<WebAccessibilityObjectPrivate*>(object.releaseRef()));
    return *this;
}

WebAccessibilityObject::operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const
{
    return PassRefPtr<WebCore::AccessibilityObject>(const_cast<WebAccessibilityObjectPrivate*>(m_private));
}

void WebAccessibilityObject::assign(WebAccessibilityObjectPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
