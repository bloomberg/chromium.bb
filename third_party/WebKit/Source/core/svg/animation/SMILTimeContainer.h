/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SMILTimeContainer_h
#define SMILTimeContainer_h

#include "core/dom/QualifiedName.h"
#include "core/svg/animation/SMILTime.h"
#include "platform/Timer.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AnimationClock;
class Document;
class SVGElement;
class SVGSMILElement;
class SVGSVGElement;

class SMILTimeContainer : public RefCounted<SMILTimeContainer>  {
public:
    static PassRefPtr<SMILTimeContainer> create(SVGSVGElement* owner) { return adoptRef(new SMILTimeContainer(owner)); }
    ~SMILTimeContainer();

    void schedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
    void unschedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
    void notifyIntervalsChanged();

    SMILTime elapsed() const;

    bool isPaused() const;
    bool isStarted() const;

    void begin();
    void pause();
    void resume();
    void setElapsed(SMILTime);

    void serviceAnimations(double monotonicAnimationStartTime);
    bool hasAnimations() const;

    void setDocumentOrderIndexesDirty() { m_documentOrderIndexesDirty = true; }

private:
    SMILTimeContainer(SVGSVGElement* owner);

    bool isTimelineRunning() const;
    void scheduleAnimationFrame(SMILTime fireTime);
    void scheduleAnimationFrame();
    void cancelAnimationFrame();
    void wakeupTimerFired(Timer<SMILTimeContainer>*);
    void updateAnimations(SMILTime elapsed, bool seekToTime = false);
    void serviceOnNextFrame();

    void updateDocumentOrderIndexes();
    double lastResumeTime() const { return m_resumeTime ? m_resumeTime : m_beginTime; }

    Document& document() const;

    double m_beginTime;
    double m_pauseTime;
    double m_resumeTime;
    double m_accumulatedActiveTime;
    double m_presetStartTime;

    bool m_documentOrderIndexesDirty;
    bool m_framePending;

    OwnPtr<AnimationClock> m_animationClock;
    Timer<SMILTimeContainer> m_wakeupTimer;

    typedef pair<SVGElement*, QualifiedName> ElementAttributePair;
    typedef Vector<SVGSMILElement*> AnimationsVector;
    typedef HashMap<ElementAttributePair, OwnPtr<AnimationsVector> > GroupedAnimationsMap;
    GroupedAnimationsMap m_scheduledAnimations;

    SVGSVGElement* m_ownerSVGElement;

#ifndef NDEBUG
    bool m_preventScheduledAnimationsChanges;
#endif
};
}

#endif // SMILTimeContainer_h
