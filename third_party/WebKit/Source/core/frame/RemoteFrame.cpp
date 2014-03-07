// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrame.h"

namespace WebCore {

inline RemoteFrame::RemoteFrame(FrameHost* host, HTMLFrameOwnerElement* ownerElement)
    : Frame(host, ownerElement)
{
}

PassRefPtr<RemoteFrame> RemoteFrame::create(FrameHost* host, HTMLFrameOwnerElement* ownerElement)
{
    RefPtr<RemoteFrame> frame = adoptRef(new RemoteFrame(host, ownerElement));
    return frame.release();
}

RemoteFrame::~RemoteFrame()
{
}

} // namespace WebCore
