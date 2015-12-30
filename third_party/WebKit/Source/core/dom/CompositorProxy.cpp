// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxy.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "platform/ThreadSafeFunctional.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorMutableProperties.h"
#include "public/platform/WebTraceLocation.h"
#include <algorithm>

namespace blink {

static const struct {
    const char* name;
    WebCompositorMutableProperty property;
} allowedProperties[] = {
    { "opacity", WebCompositorMutablePropertyOpacity },
    { "scrollleft", WebCompositorMutablePropertyScrollLeft },
    { "scrolltop", WebCompositorMutablePropertyScrollTop },
    { "transform", WebCompositorMutablePropertyTransform },
};

static WebCompositorMutableProperty compositorMutablePropertyForName(const String& attributeName)
{
    for (const auto& mapping : allowedProperties) {
        if (equalIgnoringCase(mapping.name, attributeName))
            return mapping.property;
    }
    return WebCompositorMutablePropertyNone;
}

static bool isControlThread()
{
    return !isMainThread();
}

static bool isCallingCompositorFrameCallback()
{
    // TODO(sad): Check that the requestCompositorFrame callbacks are currently being called.
    return true;
}

static void decrementCompositorProxiedPropertiesForElement(uint64_t elementId, uint32_t compositorMutableProperties)
{
    ASSERT(isMainThread());
    Node* node = DOMNodeIds::nodeForId(elementId);
    if (!node)
        return;
    Element* element = toElement(node);
    element->decrementCompositorProxiedProperties(compositorMutableProperties);
}

static void incrementCompositorProxiedPropertiesForElement(uint64_t elementId, uint32_t compositorMutableProperties)
{
    ASSERT(isMainThread());
    Node* node = DOMNodeIds::nodeForId(elementId);
    if (!node)
        return;
    Element* element = toElement(node);
    element->incrementCompositorProxiedProperties(compositorMutableProperties);
}

static bool raiseExceptionIfMutationNotAllowed(ExceptionState& exceptionState)
{
    if (!isControlThread()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "Cannot mutate a proxy attribute from the main page.");
        return true;
    }
    if (!isCallingCompositorFrameCallback()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "Cannot mutate a proxy attribute outside of a requestCompositorFrame callback.");
        return true;
    }
    return false;
}

static uint32_t compositorMutablePropertiesFromNames(const Vector<String>& attributeArray)
{
    uint32_t properties = 0;
    for (const auto& attribute : attributeArray) {
        properties |= static_cast<uint32_t>(compositorMutablePropertyForName(attribute));
    }
    return properties;
}

#if ENABLE(ASSERT)
static bool sanityCheckMutableProperties(uint32_t properties)
{
    // Ensures that we only have bits set for valid mutable properties.
    uint32_t sanityCheckProperties = properties;
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(allowedProperties); ++i) {
        sanityCheckProperties &= ~static_cast<uint32_t>(allowedProperties[i].property);
    }
    return !sanityCheckProperties;
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

CompositorProxy* CompositorProxy::create(uint64_t elementId, uint32_t compositorMutableProperties)
{
    return new CompositorProxy(elementId, compositorMutableProperties);
}

CompositorProxy::CompositorProxy(Element& element, const Vector<String>& attributeArray)
    : m_elementId(DOMNodeIds::idForNode(&element))
    , m_compositorMutableProperties(compositorMutablePropertiesFromNames(attributeArray))
{
    ASSERT(isMainThread());
    ASSERT(m_compositorMutableProperties);
    ASSERT(sanityCheckMutableProperties(m_compositorMutableProperties));

    incrementCompositorProxiedPropertiesForElement(m_elementId, m_compositorMutableProperties);
}

CompositorProxy::CompositorProxy(uint64_t elementId, uint32_t compositorMutableProperties)
    : m_elementId(elementId)
    , m_compositorMutableProperties(compositorMutableProperties)
{
    ASSERT(isControlThread());
    ASSERT(sanityCheckMutableProperties(m_compositorMutableProperties));
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&incrementCompositorProxiedPropertiesForElement, m_elementId, m_compositorMutableProperties));
}

CompositorProxy::~CompositorProxy()
{
    if (m_connected)
        disconnect();
}

bool CompositorProxy::supports(const String& attributeName) const
{
    return !!(m_compositorMutableProperties & static_cast<uint32_t>(compositorMutablePropertyForName(attributeName)));
}

double CompositorProxy::opacity(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyOpacity), exceptionState))
        return 0.0;
    return m_opacity;
}

double CompositorProxy::scrollLeft(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyScrollLeft), exceptionState))
        return 0.0;
    return m_scrollLeft;
}

double CompositorProxy::scrollTop(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyScrollTop), exceptionState))
        return 0.0;
    return m_scrollTop;
}

DOMMatrix* CompositorProxy::transform(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return nullptr;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyTransform), exceptionState))
        return nullptr;
    return m_transform;
}

void CompositorProxy::setOpacity(double opacity, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyOpacity), exceptionState))
        return;
    m_opacity = std::min(1., std::max(0., opacity));
    m_mutatedProperties |= static_cast<uint32_t>(WebCompositorMutablePropertyTransform);
}

void CompositorProxy::setScrollLeft(double scrollLeft, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyScrollLeft), exceptionState))
        return;
    m_scrollLeft = scrollLeft;
    m_mutatedProperties |= static_cast<uint32_t>(WebCompositorMutablePropertyScrollLeft);
}

void CompositorProxy::setScrollTop(double scrollTop, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyScrollTop), exceptionState))
        return;
    m_scrollTop = scrollTop;
    m_mutatedProperties |= static_cast<uint32_t>(WebCompositorMutablePropertyScrollTop);
}

void CompositorProxy::setTransform(DOMMatrix* transform, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(static_cast<uint32_t>(WebCompositorMutablePropertyTransform), exceptionState))
        return;
    m_transform = transform;
    m_mutatedProperties |= static_cast<uint32_t>(WebCompositorMutablePropertyTransform);
}

bool CompositorProxy::raiseExceptionIfNotMutable(uint32_t property, ExceptionState& exceptionState) const
{
    if (m_connected && (m_compositorMutableProperties & property))
        return false;
    exceptionState.throwDOMException(NoModificationAllowedError,
        m_connected ? "Attempted to mutate non-mutable attribute." : "Attempted to mutate attribute on a disconnected proxy.");
    return true;
}

void CompositorProxy::disconnect()
{
    m_connected = false;
    if (isMainThread())
        decrementCompositorProxiedPropertiesForElement(m_elementId, m_compositorMutableProperties);
    else
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&decrementCompositorProxiedPropertiesForElement, m_elementId, m_compositorMutableProperties));
}

} // namespace blink
