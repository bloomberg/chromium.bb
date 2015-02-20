// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "config.h"
#include "web/RemoteBridgeFrameOwner.h"

namespace blink {

RemoteBridgeFrameOwner::RemoteBridgeFrameOwner(PassRefPtrWillBeRawPtr<WebLocalFrameImpl> frame, SandboxFlags flags)
    : m_frame(frame)
    , m_sandboxFlags(flags)
{
}

DEFINE_TRACE(RemoteBridgeFrameOwner)
{
    visitor->trace(m_frame);
    FrameOwner::trace(visitor);
}

void RemoteBridgeFrameOwner::dispatchLoad()
{
    // FIXME: Implement. Most likely goes through m_frame->client().
}

} // namespace blink
