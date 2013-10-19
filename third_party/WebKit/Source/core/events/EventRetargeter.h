/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef EventRetargeter_h
#define EventRetargeter_h

#include "core/dom/ContainerNode.h"
#include "core/events/EventContext.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class EventTarget;
class FocusEvent;
class MouseEvent;
class Node;
class TouchEvent;
class TreeScope;

class EventRetargeter {
public:
    static void adjustForMouseEvent(Node*, MouseEvent&);
    static void adjustForFocusEvent(Node*, FocusEvent&);
    typedef Vector<RefPtr<TouchList> > EventPathTouchLists;
    static void adjustForTouchEvent(Node*, TouchEvent&);

private:
    typedef Vector<RefPtr<EventTarget> > AdjustedTargets;
    typedef HashMap<TreeScope*, EventTarget*> RelatedTargetMap;
    enum EventWithRelatedTargetDispatchBehavior {
        StopAtBoundaryIfNeeded,
        DoesNotStopAtBoundary
    };
    static void calculateEventPath(Node*, Event*);
    static void calculateAdjustedEventPathForEachNode(EventPath&);

    static void adjustForRelatedTarget(const Node*, EventTarget* relatedTarget, EventPath&);
    static void calculateAdjustedNodes(const Node*, const Node* relatedNode, EventWithRelatedTargetDispatchBehavior, EventPath&, AdjustedTargets&);
    static void buildRelatedNodeMap(const Node*, RelatedTargetMap&);
    static EventTarget* findRelatedNode(TreeScope*, RelatedTargetMap&);
    static void adjustTouchList(const Node*, const TouchList*, const EventPath&, EventPathTouchLists&);
};

}

#endif // EventRetargeter_h
