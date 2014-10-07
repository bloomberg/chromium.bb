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

#include "config.h"

#include "core/rendering/svg/RenderSVGResourceSolidColor.h"

#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

const RenderSVGResourceType RenderSVGResourceSolidColor::s_resourceType = SolidColorResourceType;

RenderSVGResourceSolidColor::RenderSVGResourceSolidColor()
{
}

RenderSVGResourceSolidColor::~RenderSVGResourceSolidColor()
{
}

bool RenderSVGResourceSolidColor::applyResource(RenderObject* object, RenderStyle* style, GraphicsContext* context, RenderSVGResourceModeFlags resourceMode)
{
    ASSERT_UNUSED(object, object);
    ASSERT_UNUSED(style, style);
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, resourceMode);

    if (resourceMode & ApplyToFillMode)
        context->setFillColor(m_color);
    else if (resourceMode & ApplyToStrokeMode)
        context->setStrokeColor(m_color);

    updateGraphicsContext(context, style, *object, resourceMode);
    return true;
}

}
