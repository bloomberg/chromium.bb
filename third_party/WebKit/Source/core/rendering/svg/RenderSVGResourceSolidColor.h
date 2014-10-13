/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef RenderSVGResourceSolidColor_h
#define RenderSVGResourceSolidColor_h

#include "core/rendering/svg/RenderSVGResource.h"
#include "platform/graphics/Color.h"

namespace blink {

class RenderSVGResourceSolidColor final : public RenderSVGResource {
public:
    RenderSVGResourceSolidColor();
    virtual ~RenderSVGResourceSolidColor();

    virtual SVGPaintServer preparePaintServer(RenderObject*, RenderStyle*, RenderSVGResourceModeFlags) override;

    virtual RenderSVGResourceType resourceType() const override { return s_resourceType; }
    static const RenderSVGResourceType s_resourceType;

    const Color& color() const { return m_color; }
    void setColor(const Color& color) { m_color = color; }

private:
    Color m_color;
};

}

#endif
