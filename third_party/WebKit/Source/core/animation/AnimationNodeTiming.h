// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationNodeTiming_h
#define AnimationNodeTiming_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/animation/AnimationNode.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class UnrestrictedDoubleOrString;

class AnimationNodeTiming : public RefCountedWillBeGarbageCollectedFinalized<AnimationNodeTiming>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<AnimationNodeTiming> create(AnimationNode* parent);
    double delay();
    double endDelay();
    String fill();
    double iterationStart();
    double iterations();
    void duration(UnrestrictedDoubleOrString&);
    double playbackRate();
    String direction();
    String easing();

    void setDelay(double);
    void setEndDelay(double);
    void setFill(String);
    void setIterationStart(double);
    void setIterations(double);
    void setDuration(const UnrestrictedDoubleOrString&);
    void setPlaybackRate(double);
    void setDirection(String);
    void setEasing(String);

    DECLARE_TRACE();

private:
    RefPtrWillBeMember<AnimationNode> m_parent;
    explicit AnimationNodeTiming(AnimationNode*);
};

} // namespace blink

#endif
