// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimedItemTiming_h
#define TimedItemTiming_h

#include "core/animation/TimedItem.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class TimedItemTiming : public RefCountedWillBeGarbageCollectedFinalized<TimedItemTiming> {
public:
    static PassRefPtrWillBeRawPtr<TimedItemTiming> create(TimedItem* parent);
    double delay();
    double endDelay();
    String fill();
    double iterationStart();
    double iterations();
    void getDuration(String propertyName, bool& element0Enabled, double& element0, bool& element1Enabled, String& element1);
    double playbackRate();
    String direction();
    String easing();

    void setDelay(double);
    void setEndDelay(double);
    void setFill(String);
    void setIterationStart(double);
    void setIterations(double);
    bool setDuration(String name, double duration);
    void setPlaybackRate(double);
    void setDirection(String);
    void setEasing(String);

    void trace(Visitor*);

private:
    RefPtrWillBeMember<TimedItem> m_parent;
    explicit TimedItemTiming(TimedItem*);
};

} // namespace WebCore

#endif
