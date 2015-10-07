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
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleFetchedImageSet.h"
#include "core/style/StyleGeneratedImage.h"
#include "core/style/StyleImage.h"
#include "core/style/StylePendingImage.h"
#include "platform/graphics/filters/FilterOperation.h"

namespace blink {

ElementStyleResources::ElementStyleResources()
    : m_deviceScaleFactor(1)
{
}

PassRefPtrWillBeRawPtr<StyleImage> ElementStyleResources::styleImage(Document& document, CSSPropertyID property, const CSSValue& value)
{
    if (value.isImageValue())
        return cachedOrPendingFromValue(document, property, toCSSImageValue(value));

    if (value.isImageGeneratorValue())
        return generatedOrPendingFromValue(property, toCSSImageGeneratorValue(value));

    if (value.isImageSetValue())
        return setOrPendingFromValue(property, toCSSImageSetValue(value));

    if (value.isCursorImageValue())
        return cursorOrPendingFromValue(property, toCSSCursorImageValue(value));

    return nullptr;
}

PassRefPtrWillBeRawPtr<StyleImage> ElementStyleResources::generatedOrPendingFromValue(CSSPropertyID property, const CSSImageGeneratorValue& value)
{
    if (value.isPending()) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return StyleGeneratedImage::create(value);
}

PassRefPtrWillBeRawPtr<StyleImage> ElementStyleResources::setOrPendingFromValue(CSSPropertyID property, const CSSImageSetValue& value)
{
    if (value.isCachePending(m_deviceScaleFactor)) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return value.cachedImageSet(m_deviceScaleFactor);
}

PassRefPtrWillBeRawPtr<StyleImage> ElementStyleResources::cachedOrPendingFromValue(Document& document, CSSPropertyID property, const CSSImageValue& value)
{
    if (value.isCachePending()) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    value.restoreCachedResourceIfNeeded(document);
    return value.cachedImage();
}

PassRefPtrWillBeRawPtr<StyleImage> ElementStyleResources::cursorOrPendingFromValue(CSSPropertyID property, const CSSCursorImageValue& value)
{
    if (value.isCachePending(m_deviceScaleFactor)) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return value.cachedImage(m_deviceScaleFactor);
}

void ElementStyleResources::clearPendingImageProperties()
{
    m_pendingImageProperties.clear();
}

void ElementStyleResources::clearPendingSVGDocuments()
{
    m_pendingSVGDocuments.clear();
}

void ElementStyleResources::addPendingSVGDocument(FilterOperation* filterOperation, CSSSVGDocumentValue* cssSVGDocumentValue)
{
    m_pendingSVGDocuments.set(filterOperation, cssSVGDocumentValue);
}

}
