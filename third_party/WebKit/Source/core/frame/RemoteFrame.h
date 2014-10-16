// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrame_h
#define RemoteFrame_h

#include "core/frame/Frame.h"

namespace blink {

class Event;
class RemoteFrameClient;
class RemoteFrameView;

class RemoteFrame: public Frame {
public:
    static PassRefPtrWillBeRawPtr<RemoteFrame> create(RemoteFrameClient*, FrameHost*, FrameOwner*);
    virtual bool isRemoteFrame() const override { return true; }

    virtual ~RemoteFrame();

    virtual void navigate(Document& originDocument, const KURL&, bool lockBackForwardList) override;
    virtual void detach() override;

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    void forwardInputEvent(Event*);

    void setView(PassRefPtrWillBeRawPtr<RemoteFrameView>);
    void createView();

    RemoteFrameView* view() const;

    void trace(Visitor*) override;

private:
    RemoteFrame(RemoteFrameClient*, FrameHost*, FrameOwner*);

    RemoteFrameClient* remoteFrameClient() const;

    RefPtrWillBeMember<RemoteFrameView> m_view;
};

inline RemoteFrameView* RemoteFrame::view() const
{
    return m_view.get();
}

DEFINE_TYPE_CASTS(RemoteFrame, Frame, remoteFrame, remoteFrame->isRemoteFrame(), remoteFrame.isRemoteFrame());

} // namespace blink

#endif // RemoteFrame_h
