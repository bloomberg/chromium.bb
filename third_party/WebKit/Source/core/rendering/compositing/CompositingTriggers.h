/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef CompositingTriggers_h
#define CompositingTriggers_h

namespace WebCore {

enum CompositingTrigger {
    ThreeDTransformTrigger = 1 << 0,
    VideoTrigger = 1 << 1,
    PluginTrigger = 1 << 2,
    CanvasTrigger = 1 << 3,
    AnimationTrigger = 1 << 4,
    FilterTrigger = 1 << 5,
    ScrollableInnerFrameTrigger = 1 << 6,
    GPURasterizationTrigger = 1 << 7,
    // FIXME: This is a temporary trigger for enabling the old, opt-in path for
    // accelerated overflow scroll. It should be removed once the "universal"
    // path is ready (crbug.com/254111).
    LegacyOverflowScrollTrigger = 1 << 8,
    OverflowScrollTrigger = 1 << 9,
    ViewportConstrainedPositionedTrigger = 1 << 10,
    AllCompositingTriggers = 0xFFFFFFFF,
};

typedef unsigned CompositingTriggerFlags;

}

#endif
