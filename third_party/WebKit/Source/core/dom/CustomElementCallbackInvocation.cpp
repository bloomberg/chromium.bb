/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "core/dom/CustomElementCallbackInvocation.h"

#include "core/dom/CustomElementLifecycleCallbacks.h"

namespace WebCore {

class AttributeChangedInvocation : public CustomElementCallbackInvocation {
public:
    AttributeChangedInvocation(const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue);

private:
    virtual void dispatch(CustomElementLifecycleCallbacks*, Element*) OVERRIDE;

    AtomicString m_name;
    AtomicString m_oldValue;
    AtomicString m_newValue;
};

class CreatedInvocation : public CustomElementCallbackInvocation {
public:
    CreatedInvocation() { }
private:
    virtual void dispatch(CustomElementLifecycleCallbacks*, Element*) OVERRIDE;
};

PassOwnPtr<CustomElementCallbackInvocation> CustomElementCallbackInvocation::createCreatedInvocation()
{
    return adoptPtr(new CreatedInvocation());
}

PassOwnPtr<CustomElementCallbackInvocation> CustomElementCallbackInvocation::createAttributeChangedInvocation(const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    return adoptPtr(new AttributeChangedInvocation(name, oldValue, newValue));
}

AttributeChangedInvocation::AttributeChangedInvocation(const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
    : m_name(name)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
}

void AttributeChangedInvocation::dispatch(CustomElementLifecycleCallbacks* callbacks, Element* element)
{
    callbacks->attributeChanged(element, m_name, m_oldValue, m_newValue);
}

void CreatedInvocation::dispatch(CustomElementLifecycleCallbacks* callbacks, Element* element)
{
    callbacks->created(element);
}

} // namespace WebCore
