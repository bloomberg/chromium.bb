/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#include "core/platform/graphics/gpu/SharedGraphicsContext3D.h"

#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/MainThread.h"

namespace WebCore {

class SharedGraphicsContext3DImpl {
public:
    SharedGraphicsContext3DImpl() : m_context(0) { }

    PassRefPtr<GraphicsContext3D> getOrCreateContext()
    {
        bool wasCreated = false;

        OwnPtr<WebKit::WebGraphicsContext3DProvider> provider = adoptPtr(WebKit::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());

        WebKit::WebGraphicsContext3D* webContext = 0;
        GrContext* grContext = 0;

        if (provider) {
            webContext = provider->context3d();
            grContext = provider->grContext();
        }

        if (webContext && grContext) {
            WebKit::WebGraphicsContext3D* oldWebContext = m_context ? m_context->webContext() : 0;
            GrContext* oldGrContext = m_context ? m_context->grContext() : 0;
            if (webContext != oldWebContext || grContext != oldGrContext)
                m_context.clear();

            if (!m_context) {
                m_context = GraphicsContext3D::createGraphicsContextFromProvider(provider.release());
                wasCreated = true;
            }
        }

        if (m_context && wasCreated)
            m_context->extensions()->pushGroupMarkerEXT("SharedGraphicsContext");
        return m_context;
    }

private:
    RefPtr<GraphicsContext3D> m_context;
};

PassRefPtr<GraphicsContext3D> SharedGraphicsContext3D::get()
{
    DEFINE_STATIC_LOCAL(SharedGraphicsContext3DImpl, impl, ());
    return impl.getOrCreateContext();
}

}  // namespace WebCore

