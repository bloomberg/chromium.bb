/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef TouchAdjustment_h
#define TouchAdjustment_h

#include "base/memory/scoped_refptr.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Node;

bool FindBestClickableCandidate(Node*& target_node,
                                IntPoint& target_point,
                                const IntPoint& touch_hotspot,
                                const IntRect& touch_area,
                                const HeapVector<Member<Node>>&);
bool FindBestContextMenuCandidate(Node*& target_node,
                                  IntPoint& target_point,
                                  const IntPoint& touch_hotspot,
                                  const IntRect& touch_area,
                                  const HeapVector<Member<Node>>&);
bool FindBestZoomableArea(Node*& target_node,
                          IntRect& target_area,
                          const IntPoint& touch_hotspot,
                          const IntRect& touch_area,
                          const HeapVector<Member<Node>>&);
// FIXME: Implement the similar functions for other gestures here as well.

}  // namespace blink

#endif
