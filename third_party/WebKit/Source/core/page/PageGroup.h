/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageGroup_h
#define PageGroup_h

#include "core/page/InjectedStyleSheet.h"
#include "platform/Supplementable.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

    class Page;

    // FIXME: This is really more of a "Settings Group" than a Page Group.
    // It has nothing to do with Page. There is one shared PageGroup
    // in the renderer process, which all normal Pages belong to. There are also
    // additional private PageGroups for SVGImage, Inspector Overlay, etc.

    // This really only exists to service InjectedStyleSheets at this point.
    // InjectedStyleSheets could instead just use a global, and StyleEngine
    // could be taught to look for InjectedStyleSheets when resolving style
    // for normal frames.
    class PageGroup : public Supplementable<PageGroup>, public RefCounted<PageGroup> {
        WTF_MAKE_NONCOPYABLE(PageGroup); WTF_MAKE_FAST_ALLOCATED;
    public:
        ~PageGroup();

        static PassRefPtr<PageGroup> create() { return adoptRef(new PageGroup()); }
        static PageGroup* sharedGroup();

        const HashSet<Page*>& pages() const { return m_pages; }

        void addPage(Page*);
        void removePage(Page*);

        void injectStyleSheet(const String& source, const Vector<String>& whitelist, StyleInjectionTarget);
        void removeInjectedStyleSheets();

        const InjectedStyleSheetVector& injectedStyleSheets() const { return m_injectedStyleSheets; }

    private:
        PageGroup();

        void invalidatedInjectedStyleSheetCacheInAllFrames();

        HashSet<Page*> m_pages;
        InjectedStyleSheetVector m_injectedStyleSheets;
    };

} // namespace WebCore

#endif // PageGroup_h
