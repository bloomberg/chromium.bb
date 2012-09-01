// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorInputHandlerImpl_h
#define WebCompositorInputHandlerImpl_h

#include "CCGestureCurve.h"
#include "CCInputHandler.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebCompositorInputHandler.h"
#include "WebInputEvent.h"
#include <public/WebCompositor.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WTF {
class Mutex;
}

namespace WebCore {
class IntPoint;
class CCGestureCurveTarget;
class CCInputHandlerClient;
class CCThread;
}

namespace WebKit {

class WebCompositorInputHandlerClient;

class WebCompositorInputHandlerImpl : public WebCompositorInputHandler, public WebCore::CCInputHandler, public WebCore::CCGestureCurveTarget {
    WTF_MAKE_NONCOPYABLE(WebCompositorInputHandlerImpl);
public:
    static PassOwnPtr<WebCompositorInputHandlerImpl> create(WebCore::CCInputHandlerClient*);
    static WebCompositorInputHandler* fromIdentifier(int identifier);

    virtual ~WebCompositorInputHandlerImpl();

    // WebCompositorInputHandler implementation.
    virtual void setClient(WebCompositorInputHandlerClient*);
    virtual void handleInputEvent(const WebInputEvent&);

    // WebCore::CCInputHandler implementation.
    virtual int identifier() const;
    virtual void animate(double monotonicTime);

    // WebCore::CCGestureCurveTarget implementation.
    virtual void scrollBy(const WebCore::IntPoint&);

private:
    explicit WebCompositorInputHandlerImpl(WebCore::CCInputHandlerClient*);

    enum EventDisposition { DidHandle, DidNotHandle, DropEvent };
    // This function processes the input event and determines the disposition, but does not make
    // any calls out to the WebCompositorInputHandlerClient. Some input types defer to helpers.
    EventDisposition handleInputEventInternal(const WebInputEvent&);

    EventDisposition handleGestureFling(const WebGestureEvent&);

    // Returns true if we actually had an active fling to cancel.
    bool cancelCurrentFling();

    OwnPtr<WebCore::CCActiveGestureAnimation> m_wheelFlingAnimation;
    // Parameters for the active fling animation, stored in case we need to transfer it out later.
    WebActiveWheelFlingParameters m_wheelFlingParameters;

    WebCompositorInputHandlerClient* m_client;
    int m_identifier;
    WebCore::CCInputHandlerClient* m_inputHandlerClient;

#ifndef NDEBUG
    bool m_expectScrollUpdateEnd;
    bool m_expectPinchUpdateEnd;
#endif
    bool m_gestureScrollStarted;

    static int s_nextAvailableIdentifier;
    static HashSet<WebCompositorInputHandlerImpl*>* s_compositors;
};

}

#endif // WebCompositorImpl_h
