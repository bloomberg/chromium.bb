/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSAnimations_h
#define CSSAnimations_h

#include "core/animation/Animation.h"
#include "core/animation/InertAnimation.h"
#include "core/animation/Player.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/platform/animation/CSSAnimationData.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class CSSAnimationDataList;
class Element;
class RenderObject;
class StyleResolver;

// Applied to scopes where an animation update will be added as pending and should then be applied (eg. Element style recalc).
class CSSAnimationUpdateScope FINAL {
public:
    CSSAnimationUpdateScope(Element*);
    ~CSSAnimationUpdateScope();
private:
    Element* m_target;
};

class CSSAnimationUpdate FINAL {
public:
    void startAnimation(AtomicString& animationName, const HashSet<RefPtr<InertAnimation> >& animations)
    {
        NewAnimation newAnimation;
        newAnimation.name = animationName;
        newAnimation.animations = animations;
        m_newAnimations.append(newAnimation);
    }
    // Returns whether player has been cancelled and should be filtered during style application.
    bool isCancelled(const Player* player) const { return m_cancelledAnimationPlayers.contains(player); }
    void cancelAnimation(const AtomicString& name, const HashSet<RefPtr<Player> >& players)
    {
        m_cancelledAnimationNames.append(name);
        for (HashSet<RefPtr<Player> >::const_iterator iter = players.begin(); iter != players.end(); ++iter)
            m_cancelledAnimationPlayers.add(iter->get());
    }
    struct NewAnimation {
        AtomicString name;
        HashSet<RefPtr<InertAnimation> > animations;
    };
    const Vector<NewAnimation>& newAnimations() const { return m_newAnimations; }
    const Vector<AtomicString>& cancelledAnimationNames() const { return m_cancelledAnimationNames; }
private:
    // Order is significant since it defines the order in which new animations
    // will be started. Note that there may be multiple animations present
    // with the same name, due to the way in which we split up animations with
    // incomplete keyframes.
    Vector<NewAnimation> m_newAnimations;
    Vector<AtomicString> m_cancelledAnimationNames;
    HashSet<const Player*> m_cancelledAnimationPlayers;
};

class CSSAnimations FINAL {
public:
    static bool isAnimatableProperty(CSSPropertyID);
    static bool needsUpdate(const Element*, const RenderStyle*);
    static PassOwnPtr<CSSAnimationUpdate> calculateUpdate(const Element*, const RenderStyle*, const CSSAnimations*, const CSSAnimationDataList*, StyleResolver*);
    void setPendingUpdate(PassOwnPtr<CSSAnimationUpdate> update) { m_pendingUpdate = update; }
    void maybeApplyPendingUpdate(Element*);
    bool isEmpty() const { return m_animations.isEmpty() && !m_pendingUpdate; }
    void cancel();
private:
    // Note that a single animation name may map to multiple players due to
    // the way in which we split up animations with incomplete keyframes.
    // FIXME: Once the Web Animations model supports groups, we could use a
    // ParGroup to drive multiple animations from a single Player.
    typedef HashMap<AtomicString, HashSet<RefPtr<Player> > > AnimationMap;
    AnimationMap m_animations;
    OwnPtr<CSSAnimationUpdate> m_pendingUpdate;
    class EventDelegate FINAL : public TimedItem::EventDelegate {
    public:
        EventDelegate(Element* target, const AtomicString& name)
            : m_target(target)
            , m_name(name)
        {
        }
        virtual void onEventCondition(const TimedItem*, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration) OVERRIDE;
    private:
        void maybeDispatch(Document::ListenerType, AtomicString& eventName, double elapsedTime);
        Element* m_target;
        const AtomicString m_name;
    };
};

} // namespace WebCore

#endif
