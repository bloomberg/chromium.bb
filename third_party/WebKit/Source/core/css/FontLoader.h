// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontLoader_h
#define FontLoader_h

#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourcePtr.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace WebCore {

class FontResource;

class FontLoader : public RefCountedWillBeGarbageCollectedFinalized<FontLoader> {
public:
    static PassRefPtrWillBeRawPtr<FontLoader> create(ResourceFetcher* fetcher)
    {
        return adoptRefWillBeNoop(new FontLoader(fetcher));
    }
    ~FontLoader();

    void addFontToBeginLoading(FontResource*);
    void loadPendingFonts();

#if !ENABLE(OILPAN)
    void clearResourceFetcher();
#endif

    void trace(Visitor*);

private:
    explicit FontLoader(ResourceFetcher*);
    void beginLoadTimerFired(Timer<FontLoader>*);

    Timer<FontLoader> m_beginLoadingTimer;

    typedef Vector<std::pair<ResourcePtr<FontResource>, ResourceLoader::RequestCountTracker> > FontsToLoadVector;
    FontsToLoadVector m_fontsToBeginLoading;
    RawPtrWillBeMember<ResourceFetcher> m_resourceFetcher;
};

} // namespace WebCore

#endif // FontLoader_h
