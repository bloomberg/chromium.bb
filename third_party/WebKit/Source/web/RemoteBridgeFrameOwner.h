// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef RemoteBridgeFrameOwner_h
#define RemoteBridgeFrameOwner_h

#include "core/frame/FrameOwner.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/web/WebFrameOwnerProperties.h"

namespace blink {

// Helper class to bridge communication for a frame with a remote parent.
// Currently, it serves two purposes:
// 1. Allows the local frame's loader to retrieve sandbox flags associated with
//    its owner element in another process.
// 2. Trigger a load event on its owner element once it finishes a load.
class RemoteBridgeFrameOwner final : public NoBaseWillBeGarbageCollectedFinalized<RemoteBridgeFrameOwner>, public FrameOwner {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RemoteBridgeFrameOwner);
public:
    static PassOwnPtrWillBeRawPtr<RemoteBridgeFrameOwner> create(SandboxFlags flags, const WebFrameOwnerProperties& frameOwnerProperties)
    {
        return adoptPtrWillBeNoop(new RemoteBridgeFrameOwner(flags, frameOwnerProperties));
    }

    // FrameOwner overrides:
    bool isLocal() const override { return false; }
    void setContentFrame(Frame&) override;
    void clearContentFrame() override;
    SandboxFlags getSandboxFlags() const override { return m_sandboxFlags; }
    void setSandboxFlags(SandboxFlags flags) { m_sandboxFlags = flags; }
    void dispatchLoad() override;
    // TODO(dcheng): Implement.
    void renderFallbackContent() override { }
    ScrollbarMode scrollingMode() const override { return m_scrolling; }
    int marginWidth() const override { return m_marginWidth; }
    int marginHeight() const override { return m_marginHeight; }

    void setScrollingMode(WebFrameOwnerProperties::ScrollingMode);
    void setMarginWidth(int marginWidth) { m_marginWidth = marginWidth; }
    void setMarginHeight(int marginHeight) { m_marginHeight = marginHeight; }

    DECLARE_VIRTUAL_TRACE();

private:
    RemoteBridgeFrameOwner(SandboxFlags, const WebFrameOwnerProperties&);

    RawPtrWillBeMember<Frame> m_frame;
    SandboxFlags m_sandboxFlags;
    ScrollbarMode m_scrolling;
    int m_marginWidth;
    int m_marginHeight;
};

DEFINE_TYPE_CASTS(RemoteBridgeFrameOwner, FrameOwner, owner, !owner->isLocal(), !owner.isLocal());

} // namespace blink

#endif // RemoteBridgeFrameOwner_h
