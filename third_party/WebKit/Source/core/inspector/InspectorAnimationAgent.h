// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/CoreExport.h"
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

class CORE_EXPORT InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent, InspectorFrontend::Animation>, public InspectorBackendDispatcher::AnimationCommandHandler {
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
    void getPlaybackRate(ErrorString*, double* playbackRate) override;
    void setPlaybackRate(ErrorString*, double playbackRate) override;
    void setTiming(ErrorString*, const String& animationId, double duration, double delay) override;
    void seekAnimations(ErrorString*, const RefPtr<JSONArray>& animationIds, double currentTime) override;

    // API for InspectorInstrumentation
    void didCreateAnimation(unsigned);
    void didStartAnimation(Animation*);
    void didClearDocumentOfWindowObject(LocalFrame*);

    // API for InspectorFrontend
    void enable(ErrorString*) override;

    // Methods for other agents to use.
    Animation* assertAnimation(ErrorString*, const String& id);

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorAnimationAgent(InspectorPageAgent*, InspectorDOMAgent*);

    typedef TypeBuilder::Animation::Animation::Type::Enum AnimationType;

    PassRefPtr<TypeBuilder::Animation::Animation> buildObjectForAnimation(Animation&);
    PassRefPtr<TypeBuilder::Animation::Animation> buildObjectForAnimation(Animation&, AnimationType, PassRefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr);
    double normalizedStartTime(Animation&);
    AnimationTimeline& referenceTimeline();
    Animation* animationClone(Animation*);

    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    PersistentHeapHashMapWillBeHeapHashMap<String, Member<Animation>> m_idToAnimation;
    PersistentHeapHashMapWillBeHeapHashMap<String, Member<Animation>> m_idToAnimationClone;
    WillBeHeapHashMap<String, AnimationType> m_idToAnimationType;
    bool m_isCloning;
};

}

#endif // InspectorAnimationAgent_h
