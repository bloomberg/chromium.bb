/*
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "core/layout/PointerEventsHitRules.h"

#include "wtf/Assertions.h"

namespace blink {

struct SameSizeAsPointerEventsHitRules {
  unsigned bitfields;
};

static_assert(sizeof(PointerEventsHitRules) <=
                  sizeof(SameSizeAsPointerEventsHitRules),
              "PointerEventsHitRules should stay small");

PointerEventsHitRules::PointerEventsHitRules(EHitTesting hitTesting,
                                             const HitTestRequest& request,
                                             EPointerEvents pointerEvents)
    : requireVisible(false),
      requireFill(false),
      requireStroke(false),
      canHitStroke(false),
      canHitFill(false),
      canHitBoundingBox(false) {
  if (request.svgClipContent())
    pointerEvents = EPointerEvents::kFill;

  if (hitTesting == SVG_GEOMETRY_HITTESTING) {
    switch (pointerEvents) {
      case EPointerEvents::kBoundingBox:
        canHitBoundingBox = true;
        break;
      case EPointerEvents::kVisiblePainted:
      case EPointerEvents::kAuto:  // "auto" is like "visiblePainted" when in
                                   // SVG content
        requireFill = true;
        requireStroke = true;
      case EPointerEvents::kVisible:
        requireVisible = true;
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kVisibleFill:
        requireVisible = true;
        canHitFill = true;
        break;
      case EPointerEvents::kVisibleStroke:
        requireVisible = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kPainted:
        requireFill = true;
        requireStroke = true;
      case EPointerEvents::kAll:
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kFill:
        canHitFill = true;
        break;
      case EPointerEvents::kStroke:
        canHitStroke = true;
        break;
      case EPointerEvents::kNone:
        // nothing to do here, defaults are all false.
        break;
    }
  } else {
    switch (pointerEvents) {
      case EPointerEvents::kBoundingBox:
        canHitBoundingBox = true;
        break;
      case EPointerEvents::kVisiblePainted:
      case EPointerEvents::kAuto:  // "auto" is like "visiblePainted" when in
                                   // SVG content
        requireVisible = true;
        requireFill = true;
        requireStroke = true;
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kVisibleFill:
      case EPointerEvents::kVisibleStroke:
      case EPointerEvents::kVisible:
        requireVisible = true;
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kPainted:
        requireFill = true;
        requireStroke = true;
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kFill:
      case EPointerEvents::kStroke:
      case EPointerEvents::kAll:
        canHitFill = true;
        canHitStroke = true;
        break;
      case EPointerEvents::kNone:
        // nothing to do here, defaults are all false.
        break;
    }
  }
}

}  // namespace blink

// vim:ts=4:noet
