/*
 * Copyright (C) 2005, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#ifndef MediaFeatureNames_h
#define MediaFeatureNames_h

#include "wtf/text/AtomicString.h"

namespace WebCore {
namespace MediaFeatureNames {

#define CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(macro) \
    macro(deprecatedTransition, "-webkit-transition") \
    macro(color, "color") \
    macro(colorIndex, "color-index") \
    macro(grid, "grid") \
    macro(monochrome, "monochrome") \
    macro(height, "height") \
    macro(hover, "hover") \
    macro(width, "width") \
    macro(orientation, "orientation") \
    macro(aspectRatio, "aspect-ratio") \
    macro(deviceAspectRatio, "device-aspect-ratio") \
    macro(devicePixelRatio, "-webkit-device-pixel-ratio") \
    macro(deviceHeight, "device-height") \
    macro(deviceWidth, "device-width") \
    macro(maxColor, "max-color") \
    macro(maxColorIndex, "max-color-index") \
    macro(maxAspectRatio, "max-aspect-ratio") \
    macro(maxDeviceAspectRatio, "max-device-aspect-ratio") \
    macro(maxDevicePixelRatio, "-webkit-max-device-pixel-ratio") \
    macro(maxDeviceHeight, "max-device-height") \
    macro(maxDeviceWidth, "max-device-width") \
    macro(maxHeight, "max-height") \
    macro(maxMonochrome, "max-monochrome") \
    macro(maxWidth, "max-width") \
    macro(maxResolution, "max-resolution") \
    macro(minColor, "min-color") \
    macro(minColorIndex, "min-color-index") \
    macro(minAspectRatio, "min-aspect-ratio") \
    macro(minDeviceAspectRatio, "min-device-aspect-ratio") \
    macro(minDevicePixelRatio, "-webkit-min-device-pixel-ratio") \
    macro(minDeviceHeight, "min-device-height") \
    macro(minDeviceWidth, "min-device-width") \
    macro(minHeight, "min-height") \
    macro(minMonochrome, "min-monochrome") \
    macro(minWidth, "min-width") \
    macro(minResolution, "min-resolution") \
    macro(pointer, "pointer") \
    macro(resolution, "resolution") \
    macro(transform2d, "-webkit-transform-2d") \
    macro(transform3d, "-webkit-transform-3d") \
    macro(scan, "scan") \
    macro(animation, "-webkit-animation") \
    macro(viewMode, "-webkit-view-mode")

// end of macro

#ifndef CSS_MEDIAQUERY_NAMES_HIDE_GLOBALS
#define CSS_MEDIAQUERY_NAMES_DECLARE(name, str) extern const AtomicString name##MediaFeature;
CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(CSS_MEDIAQUERY_NAMES_DECLARE)
#undef CSS_MEDIAQUERY_NAMES_DECLARE
#endif

    void init();

} // namespace MediaFeatureNames
} // namespace WebCore

#endif // MediaFeatureNames_h
