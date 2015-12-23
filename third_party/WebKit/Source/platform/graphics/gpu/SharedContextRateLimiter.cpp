// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    , m_canUseSyncQueries(false)
{
    m_contextProvider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!m_contextProvider)
        return;

    WebGraphicsContext3D* context = m_contextProvider->context3d();
    if (context && !context->isContextLost()) {
        OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(context);
        // TODO(junov): when the GLES 3.0 command buffer is ready, we could use fenceSync instead
        m_canUseSyncQueries = extensionsUtil->supportsExtension("GL_CHROMIUM_sync_query");
    }
}

void SharedContextRateLimiter::tick()
{
    if (!m_contextProvider)
        return;

    WebGraphicsContext3D* context = m_contextProvider->context3d();

    if (!context || context->isContextLost())
        return;

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

void SharedContextRateLimiter::reset()
{
    if (!m_contextProvider)
        return;

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

