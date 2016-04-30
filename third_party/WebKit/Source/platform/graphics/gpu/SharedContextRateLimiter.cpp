// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedContextRateLimiter.h"

#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/Platform.h"
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
    , m_canUseSyncQueries(false)
{
    m_contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!m_contextProvider)
        return;

    gpu::gles2::GLES2Interface* gl = m_contextProvider->contextGL();
    if (gl && gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
        OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(gl);
        // TODO(junov): when the GLES 3.0 command buffer is ready, we could use fenceSync instead
        m_canUseSyncQueries = extensionsUtil->supportsExtension("GL_CHROMIUM_sync_query");
    }
}

void SharedContextRateLimiter::tick()
{
    if (!m_contextProvider)
        return;

    gpu::gles2::GLES2Interface* gl = m_contextProvider->contextGL();
    if (!gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
        return;

    m_queries.append(0);
    if (m_canUseSyncQueries)
        gl->GenQueriesEXT(1, &m_queries.last());
    if (m_canUseSyncQueries) {
        gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, m_queries.last());
        gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
    }
    if (m_queries.size() > m_maxPendingTicks) {
        if (m_canUseSyncQueries) {
            GLuint result;
            gl->GetQueryObjectuivEXT(m_queries.first(), GL_QUERY_RESULT_EXT, &result);
            gl->DeleteQueriesEXT(1, &m_queries.first());
            m_queries.removeFirst();
        } else {
            gl->Finish();
            reset();
        }
    }
}

void SharedContextRateLimiter::reset()
{
    if (!m_contextProvider)
        return;

    gpu::gles2::GLES2Interface* gl = m_contextProvider->contextGL();
    if (gl && gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
        while (m_queries.size() > 0) {
            gl->DeleteQueriesEXT(1, &m_queries.first());
            m_queries.removeFirst();
        }
    } else {
        m_queries.clear();
    }
}

} // namespace blink
