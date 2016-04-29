// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAnimationAgent_h
#define InspectorAnimationAgent_h

#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AnimationNode;
class AnimationTimeline;
class Element;
class InspectedFrames;
class InspectorCSSAgent;
class InspectorDOMAgent;
class TimingFunction;
class V8InspectorSession;

class CORE_EXPORT InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent, protocol::Frontend::Animation>, public protocol::Backend::Animation {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
public:
    InspectorAnimationAgent(InspectedFrames*, InspectorDOMAgent*, InspectorCSSAgent*, V8InspectorSession*);

    // Base agent methods.
    void restore() override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    // Protocol method implementations
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void getPlaybackRate(ErrorString*, double* playbackRate) override;
    void setPlaybackRate(ErrorString*, double playbackRate) override;
    void getCurrentTime(ErrorString*, const String& id, double* currentTime) override;
    void setPaused(ErrorString*, PassOwnPtr<protocol::Array<String>> animations, bool paused) override;
    void setTiming(ErrorString*, const String& animationId, double duration, double delay) override;
    void seekAnimations(ErrorString*, PassOwnPtr<protocol::Array<String>> animations, double currentTime) override;
    void releaseAnimations(ErrorString*, PassOwnPtr<protocol::Array<String>> animations) override;
    void resolveAnimation(ErrorString*, const String& animationId, OwnPtr<protocol::Runtime::RemoteObject>*) override;

    // API for InspectorInstrumentation
    void didCreateAnimation(unsigned);
    void animationPlayStateChanged(blink::Animation*, blink::Animation::AnimationPlayState, blink::Animation::AnimationPlayState);
    void didClearDocumentOfWindowObject(LocalFrame*);

    // Methods for other agents to use.
    blink::Animation* assertAnimation(ErrorString*, const String& id);

    DECLARE_VIRTUAL_TRACE();

private:
    using AnimationType = protocol::Animation::Animation::TypeEnum;

    PassOwnPtr<protocol::Animation::Animation> buildObjectForAnimation(blink::Animation&);
    PassOwnPtr<protocol::Animation::Animation> buildObjectForAnimation(blink::Animation&, String, PassOwnPtr<protocol::Animation::KeyframesRule> keyframeRule = nullptr);
    double normalizedStartTime(blink::Animation&);
    AnimationTimeline& referenceTimeline();
    blink::Animation* animationClone(blink::Animation*);
    String createCSSId(blink::Animation&);

    Member<InspectedFrames> m_inspectedFrames;
    Member<InspectorDOMAgent> m_domAgent;
    Member<InspectorCSSAgent> m_cssAgent;
    V8InspectorSession* m_v8Session;
    HeapHashMap<String, Member<blink::Animation>> m_idToAnimation;
    HeapHashMap<String, Member<blink::Animation>> m_idToAnimationClone;
    HashMap<String, String> m_idToAnimationType;
    bool m_isCloning;
    HashSet<String> m_clearedAnimations;
};

} // namespace blink

#endif // InspectorAnimationAgent_h
