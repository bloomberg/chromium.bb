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
#include "core/dom/CustomElement.h"

#include "core/dom/CustomElementCallbackScheduler.h"
#include "core/dom/CustomElementRegistrationContext.h"
#include "core/dom/Element.h"

namespace WebCore {

void CustomElement::define(Element* element, PassRefPtr<CustomElementDefinition> passDefinition)
{
    RefPtr<CustomElementDefinition> definition(passDefinition);
    element->setCustomElementState(Element::Defined);
    definitions().add(element, definition);
    CustomElementCallbackScheduler::scheduleCreatedCallback(definition->callbacks(), element);
}

void CustomElement::attributeDidChange(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    CustomElementCallbackScheduler::scheduleAttributeChangedCallback(definitions().get(element)->callbacks(), element, name, oldValue, newValue);
}

void CustomElement::didEnterDocument(Element* element)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    CustomElementCallbackScheduler::scheduleEnteredDocumentCallback(definitions().get(element)->callbacks(), element);
}

void CustomElement::didLeaveDocument(Element* element)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    CustomElementCallbackScheduler::scheduleLeftDocumentCallback(definitions().get(element)->callbacks(), element);
}

void CustomElement::wasDestroyed(Element* element)
{
    definitions().remove(element);

    // FIXME: Elements should only depend on their document's
    // registration context at creation; maintain a mapping to
    // registration context for upgrade candidates.
    if (element->document() && element->document()->registrationContext())
        element->document()->registrationContext()->customElementWasDestroyed(element);
}

void CustomElement::DefinitionMap::add(Element* element, PassRefPtr<CustomElementDefinition> definition)
{
    ASSERT(definition.get());
    DefinitionMap::ElementDefinitionHashMap::AddResult result = m_definitions.add(element, definition);
    ASSERT(result.isNewEntry);
}

void CustomElement::DefinitionMap::remove(Element* element)
{
    m_definitions.remove(element);
}

CustomElementDefinition* CustomElement::DefinitionMap::get(Element* element)
{
    DefinitionMap::ElementDefinitionHashMap::const_iterator it = m_definitions.find(element);
    ASSERT(it != m_definitions.end());
    return it->value.get();
}

CustomElement::DefinitionMap& CustomElement::definitions()
{
    DEFINE_STATIC_LOCAL(DefinitionMap, definitionMap, ());
    return definitionMap;
}

} // namespace WebCore
