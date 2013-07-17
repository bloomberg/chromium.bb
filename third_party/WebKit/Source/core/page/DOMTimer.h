/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef DOMTimer_h
#define DOMTimer_h

#include "bindings/v8/ScheduledAction.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/page/SuspendableTimer.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class ScriptExecutionContext;

class DOMTimer : public SuspendableTimer {
public:
    // Creates a new timer owned by the ScriptExecutionContext, starts it and returns its ID.
    static int install(ScriptExecutionContext*, PassOwnPtr<ScheduledAction>, int timeout, bool singleShot);
    static void removeByID(ScriptExecutionContext*, int timeoutID);

    virtual ~DOMTimer();

    int timeoutID() const;

    // ActiveDOMObject
    virtual void contextDestroyed() OVERRIDE;
    virtual void stop() OVERRIDE;

    // The following are essentially constants. All intervals are in seconds.
    static double hiddenPageAlignmentInterval();
    static double visiblePageAlignmentInterval();

private:
    friend class ScriptExecutionContext; // For create().

    // Should only be used by ScriptExecutionContext.
    static PassOwnPtr<DOMTimer> create(ScriptExecutionContext* context, PassOwnPtr<ScheduledAction> action, int timeout, bool singleShot, int timeoutID)
    {
        return adoptPtr(new DOMTimer(context, action, timeout, singleShot, timeoutID));
    }

    DOMTimer(ScriptExecutionContext*, PassOwnPtr<ScheduledAction>, int interval, bool singleShot, int timeoutID);
    virtual void fired();

    // Retuns timer fire time rounded to the next multiple of timer alignment interval.
    virtual double alignedFireTime(double) const;

    int m_timeoutID;
    int m_nestingLevel;
    OwnPtr<ScheduledAction> m_action;
    RefPtr<UserGestureToken> m_userGestureToken;
};

} // namespace WebCore

#endif // DOMTimer_h
