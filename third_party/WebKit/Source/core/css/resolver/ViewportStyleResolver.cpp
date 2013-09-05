/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/ViewportStyleResolver.h"

#include "CSSValueKeywords.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/dom/Document.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/ViewportArguments.h"

namespace WebCore {

ViewportStyleResolver::ViewportStyleResolver(Document* document)
    : m_document(document),
    m_hasAuthorStyle(false)
{
    ASSERT(m_document);
}

ViewportStyleResolver::~ViewportStyleResolver()
{
}

void ViewportStyleResolver::addViewportRule(StyleRuleViewport* viewportRule)
{
    StylePropertySet* propertySet = viewportRule->mutableProperties();

    unsigned propertyCount = propertySet->propertyCount();
    if (!propertyCount)
        return;

    if (!m_propertySet) {
        m_propertySet = propertySet->mutableCopy();
        return;
    }

    // We cannot use mergeAndOverrideOnConflict() here because it doesn't
    // respect the !important declaration (but addParsedProperty() does).
    for (unsigned i = 0; i < propertyCount; ++i)
        m_propertySet->addParsedProperty(propertySet->propertyAt(i).toCSSProperty());
}

void ViewportStyleResolver::clearDocument()
{
    m_document = 0;
}

void ViewportStyleResolver::resolve()
{
    if (!m_document)
        return;

    if (!m_propertySet || (!m_hasAuthorStyle && m_document->hasLegacyViewportTag())) {
        ASSERT(!m_hasAuthorStyle);
        m_propertySet = 0;
        m_document->setViewportArguments(ViewportArguments());
        return;
    }

    ViewportArguments arguments(m_hasAuthorStyle ? ViewportArguments::AuthorStyleSheet : ViewportArguments::UserAgentStyleSheet);

    arguments.userZoom = viewportArgumentValue(CSSPropertyUserZoom);
    arguments.zoom = viewportArgumentValue(CSSPropertyZoom);
    arguments.minZoom = viewportArgumentValue(CSSPropertyMinZoom);
    arguments.maxZoom = viewportArgumentValue(CSSPropertyMaxZoom);
    arguments.minWidth = viewportLengthValue(CSSPropertyMinWidth);
    arguments.maxWidth = viewportLengthValue(CSSPropertyMaxWidth);
    arguments.minHeight = viewportLengthValue(CSSPropertyMinHeight);
    arguments.maxHeight = viewportLengthValue(CSSPropertyMaxHeight);
    arguments.orientation = viewportArgumentValue(CSSPropertyOrientation);

    m_document->setViewportArguments(arguments);

    m_propertySet = 0;
    m_hasAuthorStyle = false;
}

float ViewportStyleResolver::viewportArgumentValue(CSSPropertyID id) const
{
    float defaultValue = ViewportArguments::ValueAuto;

    // UserZoom default value is CSSValueZoom, which maps to true, meaning that
    // yes, it is user scalable. When the value is set to CSSValueFixed, we
    // return false.
    if (id == CSSPropertyUserZoom)
        defaultValue = 1;

    RefPtr<CSSValue> value = m_propertySet->getPropertyCSSValue(id);
    if (!value || !value->isPrimitiveValue())
        return defaultValue;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());

    if (primitiveValue->isNumber() || primitiveValue->isPx())
        return primitiveValue->getFloatValue();

    if (primitiveValue->isFontRelativeLength())
        return primitiveValue->getFloatValue() * m_document->renderStyle()->fontDescription().computedSize();

    if (primitiveValue->isPercentage()) {
        float percentValue = primitiveValue->getFloatValue() / 100.0f;
        switch (id) {
        case CSSPropertyMaxZoom:
        case CSSPropertyMinZoom:
        case CSSPropertyZoom:
            return percentValue;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    switch (primitiveValue->getValueID()) {
    case CSSValueAuto:
        return defaultValue;
    case CSSValueLandscape:
        return ViewportArguments::ValueLandscape;
    case CSSValuePortrait:
        return ViewportArguments::ValuePortrait;
    case CSSValueZoom:
        return defaultValue;
    case CSSValueInternalExtendToZoom:
        return ViewportArguments::ValueExtendToZoom;
    case CSSValueFixed:
        return 0;
    default:
        return defaultValue;
    }
}

Length ViewportStyleResolver::viewportLengthValue(CSSPropertyID id) const
{
    ASSERT(id == CSSPropertyMaxHeight
        || id == CSSPropertyMinHeight
        || id == CSSPropertyMaxWidth
        || id == CSSPropertyMinWidth);

    RefPtr<CSSValue> value = m_propertySet->getPropertyCSSValue(id);
    if (!value || !value->isPrimitiveValue())
        return Length(); // auto

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());

    if (primitiveValue->isLength())
        return primitiveValue->computeLength<Length>(m_document->renderStyle(), m_document->renderStyle());

    if (primitiveValue->isViewportPercentageLength())
        return primitiveValue->viewportPercentageLength();

    if (primitiveValue->isPercentage())
        return Length(primitiveValue->getFloatValue(), Percent);

    switch (primitiveValue->getValueID()) {
    case CSSValueInternalExtendToZoom:
        return Length(ExtendToZoom);
    case CSSValueAuto:
        return Length();
    default:
        // Unrecognized keyword.
        ASSERT_NOT_REACHED();
        return Length(0, Fixed);
    }
}

} // namespace WebCore
