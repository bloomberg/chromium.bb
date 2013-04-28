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
#include "core/platform/LengthBox.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class FillLayer;
class CSSValue;
class CSSAnimationData;
class RenderStyle;
class StyleImage;
class StyleResolver;
class NinePieceImage;

class CSSToStyleMap {
    WTF_MAKE_NONCOPYABLE(CSSToStyleMap);
    WTF_MAKE_FAST_ALLOCATED;

public:
    CSSToStyleMap(StyleResolver* resolver) : m_resolver(resolver) { }

    void mapFillAttachment(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillClip(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillComposite(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillBlendMode(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillOrigin(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillImage(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillRepeatX(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillRepeatY(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillSize(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillXPosition(CSSPropertyID, FillLayer*, CSSValue*);
    void mapFillYPosition(CSSPropertyID, FillLayer*, CSSValue*);

    void mapAnimationDelay(CSSAnimationData*, CSSValue*);
    void mapAnimationDirection(CSSAnimationData*, CSSValue*);
    void mapAnimationDuration(CSSAnimationData*, CSSValue*);
    void mapAnimationFillMode(CSSAnimationData*, CSSValue*);
    void mapAnimationIterationCount(CSSAnimationData*, CSSValue*);
    void mapAnimationName(CSSAnimationData*, CSSValue*);
    void mapAnimationPlayState(CSSAnimationData*, CSSValue*);
    void mapAnimationProperty(CSSAnimationData*, CSSValue*);
    void mapAnimationTimingFunction(CSSAnimationData*, CSSValue*);

    void mapNinePieceImage(CSSPropertyID, CSSValue*, NinePieceImage&);
    void mapNinePieceImageSlice(CSSValue*, NinePieceImage&);
    LengthBox mapNinePieceImageQuad(CSSValue*);
    void mapNinePieceImageRepeat(CSSValue*, NinePieceImage&);

private:
    // FIXME: These accessors should be replaced by a ResolveState object
    // similar to how PaintInfo/LayoutState cache values needed for
    // the current paint/layout.
    RenderStyle* style() const;
    RenderStyle* rootElementStyle() const;
    bool useSVGZoomRules() const;

    // FIXME: This should be part of some sort of StyleImageCache object which
    // is held by the StyleResolver, and likely provided to this object
    // during the resolve.
    PassRefPtr<StyleImage> styleImage(CSSPropertyID, CSSValue*);

    StyleResolver* m_resolver;
};

}

#endif
