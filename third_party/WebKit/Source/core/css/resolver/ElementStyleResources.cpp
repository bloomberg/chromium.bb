/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/css/resolver/ElementStyleResources.h"

#include "core/css/CSSGradientValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/platform/graphics/filters/FilterOperation.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/rendering/style/StyleImage.h"
#include "core/rendering/style/StylePendingImage.h"

namespace WebCore {

ElementStyleResources::ElementStyleResources()
    : m_hasPendingShaders(false)
    , m_deviceScaleFactor(1)
{
}

PassRefPtr<StyleImage> ElementStyleResources::styleImage(const TextLinkColors& textLinkColors, Color currentColor, CSSPropertyID property, CSSValue* value)
{
    if (value->isImageValue())
        return cachedOrPendingFromValue(property, toCSSImageValue(value));

    if (value->isImageGeneratorValue()) {
        if (value->isGradientValue())
            return generatedOrPendingFromValue(property, static_cast<CSSGradientValue*>(value)->gradientWithStylesResolved(textLinkColors, currentColor).get());
        return generatedOrPendingFromValue(property, static_cast<CSSImageGeneratorValue*>(value));
    }

    if (value->isImageSetValue())
        return setOrPendingFromValue(property, toCSSImageSetValue(value));

    if (value->isCursorImageValue())
        return cursorOrPendingFromValue(property, toCSSCursorImageValue(value));

    return 0;
}

PassRefPtr<StyleImage> ElementStyleResources::generatedOrPendingFromValue(CSSPropertyID property, CSSImageGeneratorValue* value)
{
    if (value->isPending()) {
        m_pendingImageProperties.set(property, value);
        return StylePendingImage::create(value);
    }
    return StyleGeneratedImage::create(value);
}

PassRefPtr<StyleImage> ElementStyleResources::setOrPendingFromValue(CSSPropertyID property, CSSImageSetValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImageSet(m_deviceScaleFactor);
    if (image && image->isPendingImage())
        m_pendingImageProperties.set(property, value);
    return image.release();
}

PassRefPtr<StyleImage> ElementStyleResources::cachedOrPendingFromValue(CSSPropertyID property, CSSImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage();
    if (image && image->isPendingImage())
        m_pendingImageProperties.set(property, value);
    return image.release();
}

PassRefPtr<StyleImage> ElementStyleResources::cursorOrPendingFromValue(CSSPropertyID property, CSSCursorImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage(m_deviceScaleFactor);
    if (image && image->isPendingImage())
        m_pendingImageProperties.set(property, value);
    return image.release();
}

void ElementStyleResources::addPendingSVGDocument(FilterOperation* filterOperation, CSSSVGDocumentValue* cssSVGDocumentValue)
{
    m_pendingSVGDocuments.set(filterOperation, cssSVGDocumentValue);
}

void ElementStyleResources::clear()
{
    m_pendingImageProperties.clear();
    m_pendingSVGDocuments.clear();
    m_hasPendingShaders = false;
    m_deviceScaleFactor = 1;
}

}
