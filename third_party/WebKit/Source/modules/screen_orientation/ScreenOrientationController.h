// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/page/PageLifecycleObserver.h"
#include "public/platform/WebScreenOrientationType.h"

namespace WebCore {

class FrameView;

class ScreenOrientationController FINAL : public NoBaseWillBeGarbageCollected<ScreenOrientationController>, public DocumentSupplement, public PageLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(ScreenOrientationController);
public:
    blink::WebScreenOrientationType orientation() const;

    // DocumentSupplement API.
    static ScreenOrientationController& from(Document&);
    static const char* supplementName();

private:
    explicit ScreenOrientationController(Document&);
    static blink::WebScreenOrientationType computeOrientation(FrameView*);

    // Inherited from PageLifecycleObserver.
    virtual void pageVisibilityChanged() OVERRIDE;

    Document& m_document;
    blink::WebScreenOrientationType m_overrideOrientation;
};

} // namespace WebCore

#endif // ScreenOrientationController_h
