// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrame.h"

namespace WebCore {

inline RemoteFrame::RemoteFrame(PassRefPtr<FrameInit> frameInit)
    : Frame(frameInit)
{
}

PassRefPtr<RemoteFrame> RemoteFrame::create(PassRefPtr<FrameInit> frameInit)
{
    RefPtr<RemoteFrame> frame = adoptRef(new RemoteFrame(frameInit));
    return frame.release();
}

RemoteFrame::~RemoteFrame()
{
}

} // namespace WebCore
