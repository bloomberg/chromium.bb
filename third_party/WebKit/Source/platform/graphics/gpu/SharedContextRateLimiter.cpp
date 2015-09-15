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

#include "config.h"
#include "platform/graphics/gpu/SharedContextRateLimiter.h"

#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

#ifndef GL_COMMANDS_COMPLETED_CHROMIUM
#define GL_COMMANDS_COMPLETED_CHROMIUM 0x84F7
#endif

namespace blink {

PassOwnPtr<SharedContextRateLimiter> SharedContextRateLimiter::create(unsigned maxPendingTicks)
{
    return adoptPtr(new SharedContextRateLimiter(maxPendingTicks));
}

SharedContextRateLimiter::SharedContextRateLimiter(unsigned maxPendingTicks)
    : m_maxPendingTicks(maxPendingTicks)
{
    m_contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    WebGraphicsContext3D* context = m_contextProvider->context3d();
    OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(context);
    // TODO(junov): when the GLES 3.0 command buffer is ready, we could use fenceSync instead
    m_canUseSyncQueries = extensionsUtil->supportsExtension("GL_CHROMIUM_sync_query");
}

void SharedContextRateLimiter::tick()
{
    WebGraphicsContext3D* context = m_contextProvider->context3d();
    if (context && !context->isContextLost()) {
        m_queries.append(m_canUseSyncQueries ? context->createQueryEXT() : 0);
        if (m_canUseSyncQueries) {
            context->beginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, m_queries.last());
            context->endQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
        }
        if (m_queries.size() > m_maxPendingTicks) {
            if (m_canUseSyncQueries) {
                WGC3Duint result;
                context->getQueryObjectuivEXT(m_queries.first(), GL_QUERY_RESULT_EXT, &result);
                context->deleteQueryEXT(m_queries.first());
                m_queries.removeFirst();
            } else {
                context->finish();
                reset();
            }
        }
    }
}

void SharedContextRateLimiter::reset()
{
    WebGraphicsContext3D* context = m_contextProvider->context3d();
    if (context && !context->isContextLost()) {
        while (m_queries.size() > 0) {
            context->deleteQueryEXT(m_queries.first());
            m_queries.removeFirst();
        }
    } else {
        m_queries.clear();
    }
}

} // blink

