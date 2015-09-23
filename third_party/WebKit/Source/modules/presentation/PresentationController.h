// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationController_h
#define PresentationController_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationRequest.h"
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
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(PresentationController);
    WTF_MAKE_NONCOPYABLE(PresentationController);
public:
    ~PresentationController() override;

    static PassOwnPtrWillBeRawPtr<PresentationController> create(LocalFrame&, WebPresentationClient*);

    static const char* supplementName();
    static PresentationController* from(LocalFrame&);

    static void provideTo(LocalFrame&, WebPresentationClient*);

    WebPresentationClient* client();

    // Implementation of HeapSupplement.
    DECLARE_VIRTUAL_TRACE();

    // Implementation of WebPresentationController.
    void didStartDefaultSession(WebPresentationSessionClient*) override;
    void didChangeSessionState(WebPresentationSessionClient*, WebPresentationSessionState) override;
    void didReceiveSessionTextMessage(WebPresentationSessionClient*, const WebString&) override;
    void didReceiveSessionBinaryMessage(WebPresentationSessionClient*, const uint8_t* data, size_t length) override;

    // Called by the Presentation object to advertize itself to the controller.
    // The Presentation object is kept as a WeakMember in order to avoid keeping
    // it alive when it is no longer in the tree.
    void setPresentation(Presentation*);

    // Called by the Presentation object when the default request is updated
    // in order to notify the client about the change of default presentation
    // url.
    void setDefaultRequestUrl(const KURL&);

    // Handling of running sessions.
    void registerSession(PresentationSession*);

private:
    PresentationController(LocalFrame&, WebPresentationClient*);

    // Implementation of LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    // Return the session associated with the given |sessionClient| or null if
    // it doesn't exist.
    PresentationSession* findSession(WebPresentationSessionClient*);

    // The WebPresentationClient which allows communicating with the embedder.
    // It is not owned by the PresentationController but the controller will
    // set it to null when the LocalFrame will be detached at which point the
    // client can't be used.
    WebPresentationClient* m_client;

    // Default PresentationRequest used by the embedder.
    // PersistentWillBeMember<PresentationRequest> m_defaultRequest;
    WeakMember<Presentation> m_presentation;

    // The presentation sessions associated with that frame.
    // TODO(mlamouri): the PresentationController will keep any created session
    // alive until the frame is detached. These should be weak ptr so that the
    // session can be GC'd.
    PersistentHeapHashSetWillBeHeapHashSet<Member<PresentationSession>> m_sessions;
};

} // namespace blink

#endif // PresentationController_h
