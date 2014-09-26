// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClient_h
#define RemoteFrameClient_h

#include "core/frame/FrameClient.h"

namespace blink {

class ResourceRequest;

class RemoteFrameClient : public FrameClient {
public:
    virtual ~RemoteFrameClient() { }

    virtual void navigate(const ResourceRequest&, bool shouldReplaceCurrentEntry) = 0;
};

} // namespace blink

#endif // RemoteFrameClient_h
