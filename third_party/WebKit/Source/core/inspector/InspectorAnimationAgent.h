// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/animation/Animation.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AnimationNode;
class AnimationTimeline;
class Element;
class InjectedScriptManager;
class InspectedFrames;
class InspectorCSSAgent;
class InspectorDOMAgent;
class TimingFunction;

class CORE_EXPORT InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent, InspectorFrontend::Animation>, public InspectorBackendDispatcher::AnimationCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorAnimationAgent> create(InspectedFrames* inspectedFrames, InspectorDOMAgent* domAgent, InspectorCSSAgent* cssAgent, InjectedScriptManager* injectedScriptManager)
    {
        return adoptPtrWillBeNoop(new InspectorAnimationAgent(inspectedFrames, domAgent, cssAgent, injectedScriptManager));
    }

    // Base agent methods.
    void restore() override;
    void disable(ErrorString*) override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    // Protocol method implementations
    void getPlaybackRate(ErrorString*, double* playbackRate) override;
    void setPlaybackRate(ErrorString*, double playbackRate) override;
    void getCurrentTime(ErrorString*, const String& animationId, double* currentTime) override;
    void setPaused(ErrorString*, const RefPtr<JSONArray>& animationIds, bool paused) override;
    void setTiming(ErrorString*, const String& animationId, double duration, double delay) override;
    void seekAnimations(ErrorString*, const RefPtr<JSONArray>& animationIds, double currentTime) override;
    void resolveAnimation(ErrorString*, const String& animationId, RefPtr<TypeBuilder::Runtime::RemoteObject>& result) override;
    void releaseAnimations(ErrorString*, const RefPtr<JSONArray>& animationIds) override;

    // API for InspectorInstrumentation
    void didCreateAnimation(unsigned);
    void animationPlayStateChanged(Animation*, Animation::AnimationPlayState, Animation::AnimationPlayState);
    void didClearDocumentOfWindowObject(LocalFrame*);

    // API for InspectorFrontend
    void enable(ErrorString*) override;

    // Methods for other agents to use.
    Animation* assertAnimation(ErrorString*, const String& id);

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorAnimationAgent(InspectedFrames*, InspectorDOMAgent*, InspectorCSSAgent*, InjectedScriptManager*);

    typedef TypeBuilder::Animation::Animation::Type::Enum AnimationType;

    PassRefPtr<TypeBuilder::Animation::Animation> buildObjectForAnimation(Animation&);
    PassRefPtr<TypeBuilder::Animation::Animation> buildObjectForAnimation(Animation&, AnimationType, PassRefPtr<TypeBuilder::Animation::KeyframesRule> keyframeRule = nullptr);
    double normalizedStartTime(Animation&);
    AnimationTimeline& referenceTimeline();
    Animation* animationClone(Animation*);
    String createCSSId(Animation&);

    RawPtrWillBeMember<InspectedFrames> m_inspectedFrames;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    PersistentHeapHashMapWillBeHeapHashMap<String, Member<Animation>> m_idToAnimation;
    PersistentHeapHashMapWillBeHeapHashMap<String, Member<Animation>> m_idToAnimationClone;
    WillBeHeapHashMap<String, AnimationType> m_idToAnimationType;
    bool m_isCloning;
    HashSet<String> m_clearedAnimations;
};

}

#endif // InspectorAnimationAgent_h
