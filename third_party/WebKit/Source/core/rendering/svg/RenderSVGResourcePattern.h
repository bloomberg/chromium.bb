/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright 2014 The Chromium Authors. All rights reserved.
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

#ifndef RenderSVGResourcePattern_h
#define RenderSVGResourcePattern_h

#include "core/rendering/svg/RenderSVGResourcePaintServer.h"
#include "core/svg/PatternAttributes.h"

#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class AffineTransform;
class DisplayList;
class FloatRect;
class SVGPatternElement;
struct PatternData;

class RenderSVGResourcePattern final : public RenderSVGResourcePaintServer {
public:
    explicit RenderSVGResourcePattern(SVGPatternElement*);

    virtual const char* renderName() const override { return "RenderSVGResourcePattern"; }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override;
    virtual void removeClientFromCache(RenderObject*, bool markForInvalidation = true) override;

    virtual SVGPaintServer preparePaintServer(const RenderObject&) override;

    static const RenderSVGResourceType s_resourceType = PatternResourceType;
    virtual RenderSVGResourceType resourceType() const override { return s_resourceType; }

private:
    PassOwnPtr<PatternData> buildPatternData(const RenderObject&);
    PassRefPtr<DisplayList> asDisplayList(const FloatRect& tileBounds, const AffineTransform&) const;
    PatternData* patternForRenderer(const RenderObject&);

    bool m_shouldCollectPatternAttributes : 1;
    PatternAttributes m_attributes;

    // FIXME: we can almost do away with this per-object map, but not quite: the tile size can be
    // relative to the client bounding box, and it gets captured in the cached Pattern shader.
    // Hence, we need one Pattern shader per client. The display list OTOH is the same => we
    // should be able to cache a single display list per RenderSVGResourcePattern + one
    // Pattern(shader) for each client -- this would avoid re-recording when multiple clients
    // share the same pattern.
    HashMap<const RenderObject*, OwnPtr<PatternData> > m_patternMap;
};

}

#endif
