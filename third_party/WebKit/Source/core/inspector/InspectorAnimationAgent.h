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
class AnimationPlayer;
class Element;
class InspectorDOMAgent;
class StringKeyframe;

class InspectorAnimationAgent final : public InspectorBaseAgent<InspectorAnimationAgent>, public InspectorBackendDispatcher::AnimationCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorAnimationAgent> create(InspectorDOMAgent* domAgent)
    {
        return adoptPtrWillBeNoop(new InspectorAnimationAgent(domAgent));
    }

    // Base agent methods.
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void clearFrontend() override;
    void reset();

    // Protocol method implementations.
    virtual void getAnimationPlayersForNode(ErrorString*, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::Animation::AnimationPlayer> >& animationPlayersArray) override;
    virtual void pauseAnimationPlayer(ErrorString*, const String& id, RefPtr<TypeBuilder::Animation::AnimationPlayer>&) override;
    virtual void playAnimationPlayer(ErrorString*, const String& id, RefPtr<TypeBuilder::Animation::AnimationPlayer>&) override;
    virtual void setAnimationPlayerCurrentTime(ErrorString*, const String& id, double currentTime, RefPtr<TypeBuilder::Animation::AnimationPlayer>&) override;
    virtual void getAnimationPlayerState(ErrorString*, const String& id, double* currentTime, bool* isRunning) override;

    // Methods for other agents to use.
    AnimationPlayer* assertAnimationPlayer(ErrorString*, const String& id);

    virtual void trace(Visitor*) override;

private:
    InspectorAnimationAgent(InspectorDOMAgent*);

    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    InspectorFrontend::Animation* m_frontend;
    WillBeHeapHashMap<String, RefPtrWillBeMember<AnimationPlayer> > m_idToAnimationPlayer;
};

}

#endif // InspectorAnimationAgent_h
