// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameView_h
#define RemoteFrameView_h

#include "platform/Widget.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class RemoteFrame;

class RemoteFrameView : public Widget {
public:
    static PassRefPtr<RemoteFrameView> create(RemoteFrame*);

    virtual ~RemoteFrameView();

    virtual bool isRemoteFrameView() const OVERRIDE { return true; }

    RemoteFrame& frame() const
    {
        ASSERT(m_remoteFrame);
        return *m_remoteFrame;
    }

    // Override to notify remote frame that its viewport size has changed.
    virtual void frameRectsChanged() OVERRIDE;

    virtual void invalidateRect(const IntRect&) OVERRIDE;

    virtual void setFrameRect(const IntRect&) OVERRIDE;

private:
    explicit RemoteFrameView(RemoteFrame*);

    // The RefPtrWillBePersistent-cycle between RemoteFrame and its RemoteFrameView
    // is broken in the same manner as FrameView::m_frame and LocalFrame::m_view is.
    // See the FrameView::m_frame comment.
    RefPtrWillBePersistent<RemoteFrame> m_remoteFrame;
};

DEFINE_TYPE_CASTS(RemoteFrameView, Widget, widget, widget->isRemoteFrameView(), widget.isRemoteFrameView());

} // namespace blink

#endif // RemoteFrameView_h
