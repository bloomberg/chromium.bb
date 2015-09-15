// Copyright 2015 The Chromium Authors. All rights reserved.
//
// The Chromium Authors can be found at
// http://src.chromium.org/svn/trunk/src/AUTHORS
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SharedContextRateLimiter_h
#define SharedContextRateLimiter_h

#include "public/platform/WebGraphicsContext3D.h"
#include "wtf/Deque.h"
#include "wtf/OwnPtr.h"

namespace blink {

class WebGraphicsContext3DProvider;

// Purpose: to limit the amount of worked queued for execution
//   (backlog) on the GPU by blocking the main thread to allow the GPU
//   to catch up. The Prevents unsynchronized tight animation loops
//   from cause a GPU denial of service.
//
// How it works: The rate limiter uses GPU fences to mark each tick
//   and makes sure there are never more that 'maxPendingTicks' fences
//   that are awaiting completion. On platforms that do not support
//   fences, we use glFinish instead. glFinish will only be called in
//   unsynchronized cases that submit more than maxPendingTicks animation
//   tick per compositor frame, which should be quite rare.
//
// How to use it: Each unit of work that constitutes a complete animation
//   frame must call tick(). reset() must be called when the animation
//   is consumed by committing to the compositor. Several rate limiters can
//   be used concurrently: they will each use their own sequences of
//   fences which may be interleaved. When the graphics context is lost
//   and later restored, the existing rate limiter must be destroyed and
//   a new one created.

class SharedContextRateLimiter {
public:
    static PassOwnPtr<SharedContextRateLimiter> create(unsigned maxPendingTicks);
    void tick();
    void reset();
private:
    SharedContextRateLimiter(unsigned maxPendingTicks);

    OwnPtr<WebGraphicsContext3DProvider> m_contextProvider;
    Deque<WebGLId> m_queries;
    unsigned m_maxPendingTicks;
    bool m_canUseSyncQueries;
};

} // blink

#endif
