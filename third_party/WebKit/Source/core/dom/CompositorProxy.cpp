// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/CompositorProxy.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

struct AttributeFlagMapping {
    const char* name;
    unsigned length;
    CompositorProxy::Attributes attribute;
};

static AttributeFlagMapping allowedAttributes[] = {
    { "opacity", 7, CompositorProxy::Attributes::OPACITY },
    { "scrollleft", 10, CompositorProxy::Attributes::SCROLL_LEFT },
    { "scrolltop", 9, CompositorProxy::Attributes::SCROLL_TOP },
    { "touch", 5, CompositorProxy::Attributes::TOUCH },
    { "transform", 8, CompositorProxy::Attributes::TRANSFORM },
};

static bool CompareAttributeName(const AttributeFlagMapping& attribute, StringImpl* attributeLower)
{
    ASSERT(attributeLower->is8Bit());
    return memcmp(attribute.name, attributeLower->characters8(), std::min(attribute.length, attributeLower->length())) < 0;
}

static CompositorProxy::Attributes attributeFlagForName(const String& attributeName)
{
    CompositorProxy::Attributes attributeFlag = CompositorProxy::Attributes::NONE;
    const String attributeLower = attributeName.lower();
    const AttributeFlagMapping* start = allowedAttributes;
    const AttributeFlagMapping* end = allowedAttributes + WTF_ARRAY_LENGTH(allowedAttributes);
    if (attributeLower.impl()->is8Bit()) {
        const AttributeFlagMapping* match = std::lower_bound(start, end, attributeLower.impl(), CompareAttributeName);
        if (match != end)
            attributeFlag = match->attribute;
    }
    return attributeFlag;
}

static uint32_t attributesBitfieldFromNames(const Vector<String>& attributeArray)
{
    uint32_t attributesBitfield = 0;
    for (const auto& attribute : attributeArray) {
        attributesBitfield |= static_cast<uint32_t>(attributeFlagForName(attribute));
    }
    return attributesBitfield;
}

#if ENABLE(ASSERT)
static bool sanityCheckAttributeFlags(uint32_t attributeFlags)
{
    uint32_t sanityCheckAttributes = attributeFlags;
    for (unsigned i = 0; i < arraysize(allowedAttributes); ++i) {
        sanityCheckAttributes &= ~static_cast<uint32_t>(allowedAttributes[i].attribute);
    }
    return !sanityCheckAttributes;
}
#endif

CompositorProxy* CompositorProxy::create(ExecutionContext* context, Element* element, const Vector<String>& attributeArray, ExceptionState& exceptionState)
{
    if (!context->isDocument()) {
        exceptionState.throwTypeError(ExceptionMessages::failedToConstruct("CompositorProxy", "Can only be created from the main context."));
        exceptionState.throwIfNeeded();
        return nullptr;
    }

    return new CompositorProxy(*element, attributeArray);
}

CompositorProxy* CompositorProxy::create(uint64_t elementId, uint32_t attributeFlags)
{
    return new CompositorProxy(elementId, attributeFlags);
}

CompositorProxy::CompositorProxy(Element& element, const Vector<String>& attributeArray)
    : m_elementId(DOMNodeIds::idForNode(&element))
    , m_bitfieldsSupported(attributesBitfieldFromNames(attributeArray))
{
    ASSERT(isMainThread());
    ASSERT(m_bitfieldsSupported);
    ASSERT(sanityCheckAttributeFlags(m_bitfieldsSupported));
}

CompositorProxy::CompositorProxy(uint64_t elementId, uint32_t attributeFlags)
    : m_elementId(elementId)
    , m_bitfieldsSupported(attributeFlags)
{
    ASSERT(!isMainThread());
    ASSERT(sanityCheckAttributeFlags(m_bitfieldsSupported));
}

CompositorProxy::~CompositorProxy()
{
}

bool CompositorProxy::supports(const String& attributeName) const
{
    return !!(m_bitfieldsSupported & static_cast<uint32_t>(attributeFlagForName(attributeName)));
}

} // namespace blink
