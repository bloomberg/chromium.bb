/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#ifndef CSSToStyleMap_h
#define CSSToStyleMap_h

#include "CSSPropertyNames.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/platform/LengthBox.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class FillLayer;
class CSSValue;
class CSSAnimationData;
class RenderStyle;
class StyleImage;
class StyleResolverState;
class NinePieceImage;

// CSSToStyleMap is a short-lived helper object which
// given the current StyleResolverState can map
// CSSValue objects into their RenderStyle equivalents.

class CSSToStyleMap {
    WTF_MAKE_NONCOPYABLE(CSSToStyleMap);
public:
    CSSToStyleMap(const StyleResolverState& state, ElementStyleResources& elementStyleResources) : m_state(state), m_elementStyleResources(elementStyleResources) { }

    void mapFillAttachment(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillClip(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillComposite(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillBlendMode(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillOrigin(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillImage(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillRepeatX(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillRepeatY(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillSize(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillXPosition(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillYPosition(CSSPropertyID, FillLayer*, CSSValue*) const;
    void mapFillMaskSourceType(CSSPropertyID, FillLayer*, CSSValue*);

    void mapAnimationDelay(CSSAnimationData*, CSSValue*) const;
    void mapAnimationDirection(CSSAnimationData*, CSSValue*) const;
    void mapAnimationDuration(CSSAnimationData*, CSSValue*) const;
    void mapAnimationFillMode(CSSAnimationData*, CSSValue*) const;
    void mapAnimationIterationCount(CSSAnimationData*, CSSValue*) const;
    void mapAnimationName(CSSAnimationData*, CSSValue*) const;
    void mapAnimationPlayState(CSSAnimationData*, CSSValue*) const;
    void mapAnimationProperty(CSSAnimationData*, CSSValue*) const;
    void mapAnimationTimingFunction(CSSAnimationData*, CSSValue*) const;

    void mapNinePieceImage(RenderStyle* mutableStyle, CSSPropertyID, CSSValue*, NinePieceImage&);
    void mapNinePieceImageSlice(CSSValue*, NinePieceImage&) const;
    LengthBox mapNinePieceImageQuad(CSSValue*) const;
    void mapNinePieceImageRepeat(CSSValue*, NinePieceImage&) const;

private:
    const RenderStyle* style() const;
    const RenderStyle* rootElementStyle() const;
    bool useSVGZoomRules() const;

    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue*);

    // FIXME: Consider passing a StyleResolverState (or ElementResolveState)
    // as an argument instead of caching it on this object.
    const StyleResolverState& m_state;
    ElementStyleResources& m_elementStyleResources;
};

}

#endif
