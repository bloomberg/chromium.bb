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

#include "core/page/UserStyleSheet.h"
#include "core/platform/Supplementable.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

    class KURL;
    class Page;
    class SecurityOrigin;

    class PageGroup : public Supplementable<PageGroup>, public RefCounted<PageGroup> {
        WTF_MAKE_NONCOPYABLE(PageGroup); WTF_MAKE_FAST_ALLOCATED;
    public:
        ~PageGroup();

        static PassRefPtr<PageGroup> create() { return adoptRef(new PageGroup()); }
        static PageGroup* sharedGroup();
        static PageGroup* inspectorGroup();

        const HashSet<Page*>& pages() const { return m_pages; }

        void addPage(Page*);
        void removePage(Page*);

        void addUserStyleSheet(const String& source, const KURL&,
                               const Vector<String>& whitelist, const Vector<String>& blacklist,
                               UserContentInjectedFrames,
                               UserStyleLevel level = UserStyleUserLevel,
                               UserStyleInjectionTime injectionTime = InjectInExistingDocuments);

        void removeAllUserContent();

        const UserStyleSheetVector& userStyleSheets() const { return m_userStyleSheets; }

    private:
        PageGroup();

        void invalidatedInjectedStyleSheetCacheInAllFrames();

        HashSet<Page*> m_pages;
        UserStyleSheetVector m_userStyleSheets;
    };

} // namespace WebCore

#endif // PageGroup_h
