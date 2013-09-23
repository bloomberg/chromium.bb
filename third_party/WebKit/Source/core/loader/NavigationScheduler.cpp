/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/NavigationScheduler.h"

#include "bindings/v8/ScriptController.h"
#include "core/events/Event.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/history/BackForwardController.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

unsigned NavigationDisablerForBeforeUnload::s_navigationDisableCount = 0;

class ScheduledNavigation {
    WTF_MAKE_NONCOPYABLE(ScheduledNavigation); WTF_MAKE_FAST_ALLOCATED;
public:
    ScheduledNavigation(double delay, bool lockBackForwardList, bool isLocationChange)
        : m_delay(delay)
        , m_lockBackForwardList(lockBackForwardList)
        , m_isLocationChange(isLocationChange)
        , m_wasUserGesture(ScriptController::processingUserGesture())
    {
        if (m_wasUserGesture)
            m_userGestureToken = UserGestureIndicator::currentToken();
    }
    virtual ~ScheduledNavigation() { }

    virtual void fire(Frame*) = 0;

    virtual bool shouldStartTimer(Frame*) { return true; }
    virtual void didStartTimer(Frame*, Timer<NavigationScheduler>*) { }

    double delay() const { return m_delay; }
    bool lockBackForwardList() const { return m_lockBackForwardList; }
    void setLockBackForwardList(bool lockBackForwardList) { m_lockBackForwardList = lockBackForwardList; }
    bool isLocationChange() const { return m_isLocationChange; }
    PassOwnPtr<UserGestureIndicator> createUserGestureIndicator()
    {
        if (m_wasUserGesture &&  m_userGestureToken)
            return adoptPtr(new UserGestureIndicator(m_userGestureToken));
        return adoptPtr(new UserGestureIndicator(DefinitelyNotProcessingUserGesture));
    }

    virtual bool isForm() const { return false; }

protected:
    void clearUserGesture() { m_wasUserGesture = false; }

private:
    double m_delay;
    bool m_lockBackForwardList;
    bool m_isLocationChange;
    bool m_wasUserGesture;
    RefPtr<UserGestureToken> m_userGestureToken;
};

class ScheduledURLNavigation : public ScheduledNavigation {
protected:
    ScheduledURLNavigation(double delay, SecurityOrigin* securityOrigin, const String& url, const String& referrer, bool lockBackForwardList, bool isLocationChange)
        : ScheduledNavigation(delay, lockBackForwardList, isLocationChange)
        , m_securityOrigin(securityOrigin)
        , m_url(url)
        , m_referrer(referrer)
        , m_haveToldClient(false)
    {
    }

    virtual void fire(Frame* frame)
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(m_securityOrigin.get(), ResourceRequest(KURL(ParsedURLString, m_url), m_referrer), "_self");
        request.setLockBackForwardList(lockBackForwardList());
        request.setClientRedirect(true);
        frame->loader()->load(request);
    }

    virtual void didStartTimer(Frame* frame, Timer<NavigationScheduler>* timer)
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        if (frame->loader()->history()->currentItemShouldBeReplaced())
            setLockBackForwardList(true);
    }

    SecurityOrigin* securityOrigin() const { return m_securityOrigin.get(); }
    String url() const { return m_url; }
    String referrer() const { return m_referrer; }

private:
    RefPtr<SecurityOrigin> m_securityOrigin;
    String m_url;
    String m_referrer;
    bool m_haveToldClient;
};

class ScheduledRedirect : public ScheduledURLNavigation {
public:
    ScheduledRedirect(double delay, SecurityOrigin* securityOrigin, const String& url, bool lockBackForwardList)
        : ScheduledURLNavigation(delay, securityOrigin, url, String(), lockBackForwardList, false)
    {
        clearUserGesture();
    }

    virtual bool shouldStartTimer(Frame* frame) { return frame->loader()->allAncestorsAreComplete(); }

    virtual void fire(Frame* frame)
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(securityOrigin(), ResourceRequest(KURL(ParsedURLString, url()), referrer()), "_self");
        request.setLockBackForwardList(lockBackForwardList());
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), request.resourceRequest().url()))
            request.resourceRequest().setCachePolicy(ReloadIgnoringCacheData);
        request.setClientRedirect(true);
        frame->loader()->load(request);
    }
};

class ScheduledLocationChange : public ScheduledURLNavigation {
public:
    ScheduledLocationChange(SecurityOrigin* securityOrigin, const String& url, const String& referrer, bool lockBackForwardList)
        : ScheduledURLNavigation(0.0, securityOrigin, url, referrer, lockBackForwardList, true) { }
};

class ScheduledRefresh : public ScheduledURLNavigation {
public:
    ScheduledRefresh(SecurityOrigin* securityOrigin, const String& url, const String& referrer)
        : ScheduledURLNavigation(0.0, securityOrigin, url, referrer, true, true)
    {
    }

    virtual void fire(Frame* frame)
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(securityOrigin(), ResourceRequest(KURL(ParsedURLString, url()), referrer(), ReloadIgnoringCacheData), "_self");
        request.setLockBackForwardList(lockBackForwardList());
        request.setClientRedirect(true);
        frame->loader()->load(request);
    }
};

class ScheduledHistoryNavigation : public ScheduledNavigation {
public:
    explicit ScheduledHistoryNavigation(int historySteps)
        : ScheduledNavigation(0, false, true)
        , m_historySteps(historySteps)
    {
    }

    virtual void fire(Frame* frame)
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();

        if (!m_historySteps) {
            FrameLoadRequest frameRequest(frame->document()->securityOrigin(), ResourceRequest(frame->document()->url()));
            frameRequest.setLockBackForwardList(lockBackForwardList());
            // Special case for go(0) from a frame -> reload only the frame
            // To follow Firefox and IE's behavior, history reload can only navigate the self frame.
            frame->loader()->load(frameRequest);
            return;
        }
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla). We can't easily do that.
        frame->page()->backForward().goBackOrForward(m_historySteps);
    }

private:
    int m_historySteps;
};

class ScheduledFormSubmission : public ScheduledNavigation {
public:
    ScheduledFormSubmission(PassRefPtr<FormSubmission> submission, bool lockBackForwardList)
        : ScheduledNavigation(0, lockBackForwardList, true)
        , m_submission(submission)
        , m_haveToldClient(false)
    {
        ASSERT(m_submission->state());
    }

    virtual void fire(Frame* frame)
    {
        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest frameRequest(m_submission->state()->sourceDocument()->securityOrigin());
        m_submission->populateFrameLoadRequest(frameRequest);
        frameRequest.setLockBackForwardList(lockBackForwardList());
        frameRequest.setTriggeringEvent(m_submission->event());
        frameRequest.setFormState(m_submission->state());
        frame->loader()->load(frameRequest);
    }

    virtual void didStartTimer(Frame* frame, Timer<NavigationScheduler>* timer)
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        OwnPtr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        if (frame->loader()->history()->currentItemShouldBeReplaced())
            setLockBackForwardList(true);
    }

    virtual bool isForm() const { return true; }
    FormSubmission* submission() const { return m_submission.get(); }

private:
    RefPtr<FormSubmission> m_submission;
    bool m_haveToldClient;
};

NavigationScheduler::NavigationScheduler(Frame* frame)
    : m_frame(frame)
    , m_timer(this, &NavigationScheduler::timerFired)
{
}

NavigationScheduler::~NavigationScheduler()
{
}

bool NavigationScheduler::locationChangePending()
{
    return m_redirect && m_redirect->isLocationChange();
}

void NavigationScheduler::clear()
{
    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    m_timer.stop();
    m_redirect.clear();
    m_additionalFormSubmissions.clear();
}

inline bool NavigationScheduler::shouldScheduleNavigation() const
{
    return m_frame->page();
}

inline bool NavigationScheduler::shouldScheduleNavigation(const String& url) const
{
    return shouldScheduleNavigation() && (protocolIsJavaScript(url) || NavigationDisablerForBeforeUnload::isNavigationAllowed());
}

void NavigationScheduler::scheduleRedirect(double delay, const String& url)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (url.isEmpty())
        return;

    // We want a new back/forward list item if the refresh timeout is > 1 second.
    if (!m_redirect || delay <= m_redirect->delay())
        schedule(adoptPtr(new ScheduledRedirect(delay, m_frame->document()->securityOrigin(), url, delay <= 1)));
}

bool NavigationScheduler::mustLockBackForwardList(Frame* targetFrame)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.
    if (!ScriptController::processingUserGesture() && !targetFrame->document()->loadEventFinished())
        return true;

    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    return targetFrame->tree()->parent() && !targetFrame->tree()->parent()->loader()->allAncestorsAreComplete();
}

void NavigationScheduler::scheduleLocationChange(SecurityOrigin* securityOrigin, const String& url, const String& referrer, bool lockBackForwardList)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (url.isEmpty())
        return;

    lockBackForwardList = lockBackForwardList || mustLockBackForwardList(m_frame);

    FrameLoader* loader = m_frame->loader();

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change. We'll skip this
    // optimization for cross-origin navigations to minimize the navigator's ability to
    // execute timing attacks.
    if (securityOrigin->canAccess(m_frame->document()->securityOrigin())) {
        KURL parsedURL(ParsedURLString, url);
        if (parsedURL.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(m_frame->document()->url(), parsedURL)) {
            FrameLoadRequest request(securityOrigin, ResourceRequest(m_frame->document()->completeURL(url), referrer), "_self");
            request.setLockBackForwardList(lockBackForwardList);
            request.setClientRedirect(true);
            loader->load(request);
            return;
        }
    }

    schedule(adoptPtr(new ScheduledLocationChange(securityOrigin, url, referrer, lockBackForwardList)));
}

void NavigationScheduler::scheduleFormSubmission(PassRefPtr<FormSubmission> submission)
{
    ASSERT(m_frame->page());
    if (m_redirect && m_redirect->isForm()) {
        if (submission->target() != static_cast<ScheduledFormSubmission*>(m_redirect.get())->submission()->target()) {
            const String& target = submission->target().isNull() ? "" : submission->target();
            m_additionalFormSubmissions.add(target, adoptPtr(new ScheduledFormSubmission(submission, mustLockBackForwardList(m_frame))));
            return;
        }
    }
    schedule(adoptPtr(new ScheduledFormSubmission(submission, mustLockBackForwardList(m_frame))));
}

void NavigationScheduler::scheduleRefresh()
{
    if (!shouldScheduleNavigation())
        return;
    const KURL& url = m_frame->document()->url();
    if (url.isEmpty())
        return;

    schedule(adoptPtr(new ScheduledRefresh(m_frame->document()->securityOrigin(), url.string(), m_frame->loader()->outgoingReferrer())));
}

void NavigationScheduler::scheduleHistoryNavigation(int steps)
{
    if (!shouldScheduleNavigation())
        return;

    // Invalid history navigations (such as history.forward() during a new load) have the side effect of cancelling any scheduled
    // redirects. We also avoid the possibility of cancelling the current load by avoiding the scheduled redirection altogether.
    BackForwardController& backForward = m_frame->page()->backForward();
    if (steps > backForward.forwardCount() || -steps > backForward.backCount()) {
        cancel();
        return;
    }

    // In all other cases, schedule the history traversal to occur asynchronously.
    schedule(adoptPtr(new ScheduledHistoryNavigation(steps)));
}

void NavigationScheduler::timerFired(Timer<NavigationScheduler>*)
{
    if (!m_frame->page())
        return;
    if (m_frame->page()->defersLoading()) {
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
        return;
    }

    RefPtr<Frame> protect(m_frame);

    OwnPtr<ScheduledNavigation> redirect(m_redirect.release());
    HashMap<String, OwnPtr<ScheduledNavigation> > additionalFormSubmissions;
    additionalFormSubmissions.swap(m_additionalFormSubmissions);
    redirect->fire(m_frame);
    while (!additionalFormSubmissions.isEmpty()) {
        HashMap<String, OwnPtr<ScheduledNavigation> >::iterator it = additionalFormSubmissions.begin();
        OwnPtr<ScheduledNavigation> formSubmission = it->value.release();
        additionalFormSubmissions.remove(it);
        formSubmission->fire(m_frame);
    }
    InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
}

void NavigationScheduler::schedule(PassOwnPtr<ScheduledNavigation> redirect)
{
    ASSERT(m_frame->page());
    cancel();
    m_redirect = redirect;
    startTimer();
}

void NavigationScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame->page());
    if (m_timer.isActive())
        return;
    if (!m_redirect->shouldStartTimer(m_frame))
        return;

    m_timer.startOneShot(m_redirect->delay());
    m_redirect->didStartTimer(m_frame, &m_timer);
    InspectorInstrumentation::frameScheduledNavigation(m_frame, m_redirect->delay());
}

void NavigationScheduler::cancel()
{
    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    m_timer.stop();
    m_redirect.clear();
    m_additionalFormSubmissions.clear();
}

} // namespace WebCore
