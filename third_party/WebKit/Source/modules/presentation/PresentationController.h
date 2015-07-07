// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationController_h
#define PresentationController_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/Presentation.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "public/platform/modules/presentation/WebPresentationController.h"

namespace blink {

class LocalFrame;
class WebPresentationAvailabilityCallback;
class WebPresentationSessionClient;
enum class WebPresentationSessionState;

// The coordinator between the various page exposed properties and the content
// layer represented via |WebPresentationClient|.
class MODULES_EXPORT PresentationController final
    : public NoBaseWillBeGarbageCollectedFinalized<PresentationController>
    , public WillBeHeapSupplement<LocalFrame>
    , public LocalFrameLifecycleObserver
    , public WebPresentationController {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationController);
    WTF_MAKE_NONCOPYABLE(PresentationController);
public:
    virtual ~PresentationController();

    static PassOwnPtrWillBeRawPtr<PresentationController> create(LocalFrame&, WebPresentationClient*);

    static const char* supplementName();
    static PresentationController* from(LocalFrame&);

    static void provideTo(LocalFrame&, WebPresentationClient*);

    WebPresentationClient* client();

    // Implementation of HeapSupplement.
    DECLARE_VIRTUAL_TRACE();

    // Implementation of WebPresentationController.
    virtual void didStartDefaultSession(WebPresentationSessionClient*) override;
    virtual void didChangeSessionState(WebPresentationSessionClient*, WebPresentationSessionState) override;
    virtual void didReceiveSessionTextMessage(WebPresentationSessionClient*, const WebString&) override;

    // Connects the |Presentation| object with this controller.
    void setPresentation(Presentation*);

private:
    PresentationController(LocalFrame&, WebPresentationClient*);

    // Implementation of LocalFrameLifecycleObserver.
    virtual void willDetachFrameHost() override;

    WebPresentationClient* m_client;
    PersistentWillBeMember<Presentation> m_presentation;
};

} // namespace blink

#endif // PresentationController_h
