/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/page/ScopedPageLoadDeferrer.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "wtf/HashSet.h"

namespace blink {

ScopedPageLoadDeferrer::ScopedPageLoadDeferrer(Page* exclusion)
{
    for (const Page* page : Page::ordinaryPages()) {
        if (page == exclusion || page->defersLoading())
            continue;

        if (!page->mainFrame()->isLocalFrame())
            continue;

        m_deferredFrames.append(page->deprecatedLocalMainFrame());

        // Ensure that we notify the client if the initial empty document is accessed before
        // showing anything modal, to prevent spoofs while the modal window or sheet is visible.
        page->deprecatedLocalMainFrame()->loader().notifyIfInitialDocumentAccessed();
    }

    setDefersLoading(true);
    Platform::current()->currentThread()->scheduler()->suspendTimerQueue();
}

ScopedPageLoadDeferrer::~ScopedPageLoadDeferrer()
{
    setDefersLoading(false);
    Platform::current()->currentThread()->scheduler()->resumeTimerQueue();
}

void ScopedPageLoadDeferrer::setDefersLoading(bool isDeferred)
{
    for (const auto& frame : m_deferredFrames) {
        if (Page* page = frame->page())
            page->setDefersLoading(isDeferred);
    }
}

} // namespace blink
