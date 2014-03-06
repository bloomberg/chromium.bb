/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#ifndef SVGAnimatedPropertyMacros_h
#define SVGAnimatedPropertyMacros_h

#include "core/dom/Element.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGAttributeToPropertyMap.h"
#include "core/svg/properties/SVGPropertyTraits.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

// Property registration helpers
#define BEGIN_REGISTER_ANIMATED_PROPERTIES(OwnerType)
#define REGISTER_LOCAL_ANIMATED_PROPERTY(LowerProperty)
#define REGISTER_PARENT_ANIMATED_PROPERTIES(ClassName)
#define END_REGISTER_ANIMATED_PROPERTIES

// Property declaration helpers (used in SVG*.h files)
#define BEGIN_DECLARE_ANIMATED_PROPERTIES(OwnerType)
#define DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty)
#define END_DECLARE_ANIMATED_PROPERTIES

}

#endif // SVGAnimatedPropertyMacros_h
