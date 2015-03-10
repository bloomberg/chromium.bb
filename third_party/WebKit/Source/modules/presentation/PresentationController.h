// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationController_h
#define PresentationController_h

#include "core/frame/FrameDestructionObserver.h"
#include "modules/presentation/Presentation.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "public/platform/modules/presentation/WebPresentationController.h"

namespace blink {

class LocalFrame;
class WebPresentationSessionClient;

// The coordinator between the various page exposed properties and the content
// layer represented via |WebPresentationClient|.
class PresentationController final
    : public NoBaseWillBeGarbageCollectedFinalized<PresentationController>
    , public WillBeHeapSupplement<LocalFrame>
    , public FrameDestructionObserver
    , public WebPresentationController {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationController);
    WTF_MAKE_NONCOPYABLE(PresentationController);
public:
    virtual ~PresentationController();

    static PassOwnPtrWillBeRawPtr<PresentationController> create(LocalFrame&, WebPresentationClient*);

    static const char* supplementName();
    static PresentationController* from(LocalFrame&);

    static void provideTo(LocalFrame&, WebPresentationClient*);

    // Implementation of HeapSupplement.
    DECLARE_VIRTUAL_TRACE();

    // Implementation of WebPresentationController.
    virtual void didChangeAvailability(bool available) override;
    virtual bool isAvailableChangeWatched() const override;
    virtual void didStartDefaultSession(WebPresentationSessionClient*) override;

    // Called when the first listener was added to or the last listener was removed from the
    // |availablechange| event.
    void updateAvailableChangeWatched(bool watched);

    // Called when the frame wants to start a new presentation.
    void startSession(const String& presentationUrl, const String& presentationId, WebPresentationSessionClientCallbacks*);

    // Called when the frame wants to join an existing presentation.
    void joinSession(const String& presentationUrl, const String& presentationId, WebPresentationSessionClientCallbacks*);

    // Connects the |Presentation| object with this controller.
    void setPresentation(Presentation*);

private:
    PresentationController(LocalFrame&, WebPresentationClient*);

    // Implementation of FrameDestructionObserver.
    virtual void willDetachFrameHost() override;

    WebPresentationClient* m_client;
    PersistentWillBeMember<Presentation> m_presentation;
};

} // namespace blink

#endif // PresentationController_h
