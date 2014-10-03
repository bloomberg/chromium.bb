/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "core/css/invalidation/DescendantInvalidationSet.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/TracedValue.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

DescendantInvalidationSet::DescendantInvalidationSet()
    : m_allDescendantsMightBeInvalid(false)
    , m_customPseudoInvalid(false)
    , m_treeBoundaryCrossing(false)
{
}

bool DescendantInvalidationSet::invalidatesElement(Element& element) const
{
    if (m_allDescendantsMightBeInvalid)
        return true;

    if (m_tagNames && m_tagNames->contains(element.tagQName().localName())) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, InvalidationSetMatchedTagName, element.tagQName().localName());
        return true;
    }

    if (element.hasID() && m_ids && m_ids->contains(element.idForStyleResolution())) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, InvalidationSetMatchedId, element.idForStyleResolution());
        return true;
    }

    if (element.hasClass() && m_classes) {
        const SpaceSplitString& classNames = element.classNames();
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = m_classes->begin(); it != m_classes->end(); ++it) {
            if (classNames.contains(*it)) {
                TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, InvalidationSetMatchedClass, *it);
                return true;
            }
        }
    }

    if (element.hasAttributes() && m_attributes) {
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = m_attributes->begin(); it != m_attributes->end(); ++it) {
            if (element.hasAttribute(*it)) {
                TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(element, InvalidationSetMatchedAttribute, *it);
                return true;
            }
        }
    }

    return false;
}

void DescendantInvalidationSet::combine(const DescendantInvalidationSet& other)
{
    // No longer bother combining data structures, since the whole subtree is deemed invalid.
    if (wholeSubtreeInvalid())
        return;

    if (other.wholeSubtreeInvalid()) {
        setWholeSubtreeInvalid();
        return;
    }

    if (other.customPseudoInvalid())
        setCustomPseudoInvalid();

    if (other.treeBoundaryCrossing())
        setTreeBoundaryCrossing();

    if (other.m_classes) {
        WillBeHeapHashSet<AtomicString>::const_iterator end = other.m_classes->end();
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = other.m_classes->begin(); it != end; ++it)
            addClass(*it);
    }

    if (other.m_ids) {
        WillBeHeapHashSet<AtomicString>::const_iterator end = other.m_ids->end();
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = other.m_ids->begin(); it != end; ++it)
            addId(*it);
    }

    if (other.m_tagNames) {
        WillBeHeapHashSet<AtomicString>::const_iterator end = other.m_tagNames->end();
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = other.m_tagNames->begin(); it != end; ++it)
            addTagName(*it);
    }

    if (other.m_attributes) {
        WillBeHeapHashSet<AtomicString>::const_iterator end = other.m_attributes->end();
        for (WillBeHeapHashSet<AtomicString>::const_iterator it = other.m_attributes->begin(); it != end; ++it)
            addAttribute(*it);
    }
}

WillBeHeapHashSet<AtomicString>& DescendantInvalidationSet::ensureClassSet()
{
    if (!m_classes)
        m_classes = adoptPtrWillBeNoop(new WillBeHeapHashSet<AtomicString>);
    return *m_classes;
}

WillBeHeapHashSet<AtomicString>& DescendantInvalidationSet::ensureIdSet()
{
    if (!m_ids)
        m_ids = adoptPtrWillBeNoop(new WillBeHeapHashSet<AtomicString>);
    return *m_ids;
}

WillBeHeapHashSet<AtomicString>& DescendantInvalidationSet::ensureTagNameSet()
{
    if (!m_tagNames)
        m_tagNames = adoptPtrWillBeNoop(new WillBeHeapHashSet<AtomicString>);
    return *m_tagNames;
}

WillBeHeapHashSet<AtomicString>& DescendantInvalidationSet::ensureAttributeSet()
{
    if (!m_attributes)
        m_attributes = adoptPtrWillBeNoop(new WillBeHeapHashSet<AtomicString>);
    return *m_attributes;
}

void DescendantInvalidationSet::addClass(const AtomicString& className)
{
    if (wholeSubtreeInvalid())
        return;
    ensureClassSet().add(className);
}

void DescendantInvalidationSet::addId(const AtomicString& id)
{
    if (wholeSubtreeInvalid())
        return;
    ensureIdSet().add(id);
}

void DescendantInvalidationSet::addTagName(const AtomicString& tagName)
{
    if (wholeSubtreeInvalid())
        return;
    ensureTagNameSet().add(tagName);
}

void DescendantInvalidationSet::addAttribute(const AtomicString& attribute)
{
    if (wholeSubtreeInvalid())
        return;
    ensureAttributeSet().add(attribute);
}

void DescendantInvalidationSet::setWholeSubtreeInvalid()
{
    if (m_allDescendantsMightBeInvalid)
        return;

    m_allDescendantsMightBeInvalid = true;
    m_treeBoundaryCrossing = false;
    m_classes = nullptr;
    m_ids = nullptr;
    m_tagNames = nullptr;
    m_attributes = nullptr;
}

void DescendantInvalidationSet::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_classes);
    visitor->trace(m_ids);
    visitor->trace(m_tagNames);
    visitor->trace(m_attributes);
#endif
}

void DescendantInvalidationSet::toTracedValue(TracedValue* value) const
{
    value->beginDictionary();

    if (m_allDescendantsMightBeInvalid)
        value->setBoolean("allDescendantsMightBeInvalid", true);
    if (m_customPseudoInvalid)
        value->setBoolean("customPseudoInvalid", true);
    if (m_treeBoundaryCrossing)
        value->setBoolean("treeBoundaryCrossing", true);

    if (m_ids) {
        value->beginArray("ids");
        for (const auto& id : *m_ids)
            value->pushString(id);
        value->endArray();
    }

    if (m_classes) {
        value->beginArray("classes");
        for (const auto& className : *m_classes)
            value->pushString(className);
        value->endArray();
    }

    if (m_tagNames) {
        value->beginArray("tagNames");
        for (const auto& tagName : *m_tagNames)
            value->pushString(tagName);
        value->endArray();
    }

    if (m_attributes) {
        value->beginArray("ids");
        for (const auto& attribute : *m_attributes)
            value->pushString(attribute);
        value->endArray();
    }

    value->endDictionary();
}

#ifndef NDEBUG
void DescendantInvalidationSet::show() const
{
    RefPtr<TracedValue> value = TracedValue::create();
    toTracedValue(value.get());
    fprintf(stderr, "%s\n", value->asTraceFormat().ascii().data());
}
#endif // NDEBUG

} // namespace blink
