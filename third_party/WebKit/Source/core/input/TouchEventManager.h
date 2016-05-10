// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchEventManager_h
#define TouchEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEventFactory.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"


namespace blink {

class LocalFrame;
class Document;
class PlatformTouchEvent;
class PointerEventWithTarget;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager {
    DISALLOW_NEW();
public:
    class TouchInfo {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(touchNode);
            visitor->trace(targetFrame);
        }

        PlatformTouchPoint point;
        Member<Node> touchNode;
        Member<LocalFrame> targetFrame;
        FloatPoint contentPoint;
        FloatSize adjustedRadius;
        bool knownTarget;
        bool consumed;
        String region;
    };

    explicit TouchEventManager(LocalFrame*);
    ~TouchEventManager();
    DECLARE_TRACE();

    // Returns true if it succesfully generates touchInfos.
    bool generateTouchInfosAfterHittest(
        const PlatformTouchEvent&,
        HeapVector<TouchInfo>&);

    WebInputEventResult handleTouchEvent(
        const PlatformTouchEvent&,
        const HeapVector<TouchInfo>&);

    // Resets the internal state of this object.
    void clear();

    // Returns whether there is any touch on the screen.
    bool isAnyTouchActive() const;

private:

    void updateTargetAndRegionMapsForTouchStarts(HeapVector<TouchInfo>&);
    void setAllPropertiesOfTouchInfos(HeapVector<TouchInfo>&);

    WebInputEventResult dispatchTouchEvents(
        const PlatformTouchEvent&,
        const HeapVector<TouchInfo>&,
        bool allTouchesReleased);


    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |TouchEventManager::clear()|.

    const Member<LocalFrame> m_frame;

    // The target of each active touch point indexed by the touch ID.
    using TouchTargetMap = HeapHashMap<unsigned, Member<Node>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
    TouchTargetMap m_targetForTouchID;
    using TouchRegionMap = HashMap<unsigned, String, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
    TouchRegionMap m_regionForTouchID;

    // If set, the document of the active touch sequence. Unset if no touch sequence active.
    Member<Document> m_touchSequenceDocument;

    RefPtr<UserGestureToken> m_touchSequenceUserGestureToken;
    bool m_touchPressed;
    // True if waiting on first touch move after a touch start.
    bool m_waitingForFirstTouchMove;

};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::TouchEventManager::TouchInfo);

#endif // TouchEventManager_h
