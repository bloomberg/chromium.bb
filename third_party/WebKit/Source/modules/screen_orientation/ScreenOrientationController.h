// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationController_h
#define ScreenOrientationController_h

#include "core/dom/DocumentSupplementable.h"
#include "public/platform/WebScreenOrientationType.h"

namespace WebCore {

class FrameView;

class ScreenOrientationController FINAL : public NoBaseWillBeGarbageCollected<ScreenOrientationController>, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);
public:
    virtual ~ScreenOrientationController();

    blink::WebScreenOrientationType orientation() const;

    // DocumentSupplement API.
    static ScreenOrientationController& from(Document&);
    static const char* supplementName();

private:
    explicit ScreenOrientationController(Document&);
    static blink::WebScreenOrientationType computeOrientation(FrameView*);

    Document& m_document;
};

} // namespace WebCore

#endif // ScreenOrientationController_h
