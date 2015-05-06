// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/InspectorFrontend.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Animation;
class AnimationNode;
class AnimationTimeline;
class Element;
class InspectorDOMAgent;
class InspectorPageAgent;
class TimingFunction;

class InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent, InspectorFrontend::Animation>, public InspectorBackendDispatcher::AnimationCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorAnimationAgent> create(InspectorPageAgent* pageAgent, InspectorDOMAgent* domAgent)
    {
        return adoptPtrWillBeNoop(new InspectorAnimationAgent(pageAgent, domAgent));
    }

    // Base agent methods.
    void restore() override;
    void disable(ErrorString*) override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    // Protocol method implementations
    virtual void getAnimationPlayersForNode(ErrorString*, int nodeId, bool includeSubtreeAnimations, RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> >& animationPlayersArray) override;
    virtual void getPlaybackRate(ErrorString*, double* playbackRate) override;
    virtual void setPlaybackRate(ErrorString*, double playbackRate) override;
    virtual void setCurrentTime(ErrorString*, double currentTime) override;
    virtual void setTiming(ErrorString*, const String& playerId, double duration, double delay) override;

    // API for InspectorInstrumentation
    void didCreateAnimation(Animation*);
    void didCancelAnimation(Animation*);
    void didClearDocumentOfWindowObject(LocalFrame*);

    // API for InspectorFrontend
    virtual void enable(ErrorString*) override;

    // Methods for other agents to use.
    Animation* assertAnimation(ErrorString*, const String& id);

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorAnimationAgent(InspectorPageAgent*, InspectorDOMAgent*);

    typedef TypeBuilder::Animation::AnimationPlayer::Type::Enum AnimationType;

    PassRefPtr<TypeBuilder::Animation::AnimationPlayer> buildObjectForAnimationPlayer(Animation&);
    PassRefPtr<TypeBuilder::Animation::AnimationPlayer> buildObjectForAnimationPlayer(Animation&, AnimationType, PassRefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer>> buildArrayForAnimations(Element&, const WillBeHeapVector<RefPtrWillBeMember<Animation>>);
    double normalizedStartTime(Animation&);
    AnimationTimeline& referenceTimeline();

    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    WillBeHeapHashMap<String, RefPtrWillBeMember<Animation>> m_idToAnimation;
    WillBeHeapHashMap<String, AnimationType> m_idToAnimationType;
};

}

#endif // InspectorAnimationAgent_h
