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

class AnimationNode;
class AnimationPlayer;
class Element;
class InspectorDOMAgent;
class InspectorPageAgent;
class TimingFunction;

class InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent>, public InspectorBackendDispatcher::AnimationCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorAnimationAgent> create(InspectorPageAgent* pageAgent, InspectorDOMAgent* domAgent)
    {
        return adoptPtrWillBeNoop(new InspectorAnimationAgent(pageAgent, domAgent));
    }

    // Base agent methods.
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void clearFrontend() override;
    void reset();
    virtual void restore() override;

    // Protocol method implementations
    virtual void getAnimationPlayersForNode(ErrorString*, int nodeId, bool includeSubtreeAnimations, RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> >& animationPlayersArray) override;
    virtual void getPlaybackRate(ErrorString*, double* playbackRate) override;
    virtual void setPlaybackRate(ErrorString*, double playbackRate) override;
    virtual void setCurrentTime(ErrorString*, double currentTime) override;
    virtual void setTiming(ErrorString*, const String& playerId, double duration, double delay) override;

    // API for InspectorInstrumentation
    void didCreateAnimationPlayer(AnimationPlayer&);

    // API for InspectorFrontend
    virtual void enable(ErrorString*) override;

    // Methods for other agents to use.
    AnimationPlayer* assertAnimationPlayer(ErrorString*, const String& id);

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorAnimationAgent(InspectorPageAgent*, InspectorDOMAgent*);

    typedef TypeBuilder::Animation::AnimationPlayer::Type::Enum AnimationType;

    PassRefPtr<TypeBuilder::Animation::AnimationPlayer> buildObjectForAnimationPlayer(AnimationPlayer&);
    PassRefPtr<TypeBuilder::Animation::AnimationPlayer> buildObjectForAnimationPlayer(AnimationPlayer&, AnimationType, PassRefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> > buildArrayForAnimationPlayers(Element&, const WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer> >);

    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    InspectorFrontend::Animation* m_frontend;
    WillBeHeapHashMap<String, RefPtrWillBeMember<AnimationPlayer>> m_idToAnimationPlayer;
};

}

#endif // InspectorAnimationAgent_h
