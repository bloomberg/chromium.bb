// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderPagedFlowThread_h
#define RenderPagedFlowThread_h

#include "core/rendering/RenderMultiColumnFlowThread.h"

namespace blink {

// A flow thread for paged overflow. FIXME: The current implementation relies on the multicol
// implementation, but it in the long run it would be better to have what's common between
// RenderMultiColumnFlowThread and RenderPagedFlowThread in RenderFlowThread, and have both of them
// inherit from that one.
class RenderPagedFlowThread : public RenderMultiColumnFlowThread {
public:
    static RenderPagedFlowThread* createAnonymous(Document&, RenderStyle* parentStyle);

    RenderBlockFlow* pagedBlockFlow() const { return toRenderBlockFlow(parent()); }

    virtual bool isRenderPagedFlowThread() const OVERRIDE { return true; }
    virtual bool heightIsAuto() const OVERRIDE { return !columnHeightAvailable(); }
    virtual const char* renderName() const OVERRIDE;
    virtual bool needsNewWidth() const OVERRIDE;
    virtual void updateLogicalWidth() OVERRIDE;
    virtual void layout();
};

} // namespace blink

#endif // RenderPagedFlowThread_h
