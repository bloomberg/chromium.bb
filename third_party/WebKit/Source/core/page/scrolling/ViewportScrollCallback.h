// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewportScrollCallback_h
#define ViewportScrollCallback_h

#include "core/page/scrolling/ScrollStateCallback.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class Document;
class FloatSize;
class FrameHost;
class Element;
class ScrollableArea;
class ScrollState;

class ViewportScrollCallback : public ScrollStateCallback {
public:
    ViewportScrollCallback(Document&);
    ~ViewportScrollCallback();

    void handleEvent(ScrollState*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    bool shouldScrollTopControls(const FloatSize&, ScrollGranularity) const;
    ScrollableArea* getRootFrameViewport() const;

    WeakMember<Document> m_document;
};

} // namespace blink

#endif // ViewportScrollCallback_h
