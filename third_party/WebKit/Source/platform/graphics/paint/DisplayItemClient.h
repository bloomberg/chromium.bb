// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClient_h
#define DisplayItemClient_h

#include "platform/PlatformExport.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Holds a unique cache generation id of display items and paint controllers.
//
// A paint controller sets its cache generation to DisplayItemCacheGeneration::next()
// at the end of each commitNewDisplayItems, and updates the cache generation of each
// client with cached drawings by calling DisplayItemClient::setDisplayItemsCached().
// A display item is treated as validly cached in a paint controller if its cache generation
// matches the paint controller's cache generation.
//
// SPv1 only: If a display item is painted on multiple paint controllers, because cache
// generations are unique, the client's cache generation matches the last paint controller
// only. The client will be treated as invalid on other paint controllers regardless if
// it's validly cached by these paint controllers. The situation is very rare (about 0.07%
// clients were painted on multiple paint controllers) so the performance penalty is trivial.
class PLATFORM_EXPORT DisplayItemCacheGeneration {
    DISALLOW_NEW();
public:
    DisplayItemCacheGeneration() : m_value(kInvalidGeneration) { }

    void invalidate() { m_value = kInvalidGeneration; }
    static DisplayItemCacheGeneration next() { return DisplayItemCacheGeneration(s_nextGeneration++); }
    bool matches(const DisplayItemCacheGeneration& other)
    {
        return m_value != kInvalidGeneration && other.m_value != kInvalidGeneration && m_value == other.m_value;
    }

private:
    typedef uint32_t Generation;
    DisplayItemCacheGeneration(Generation value) : m_value(value) { }

    static const Generation kInvalidGeneration = 0;
    static Generation s_nextGeneration;
    Generation m_value;
};

// The interface for objects that can be associated with display items.
// A DisplayItemClient object should live at least longer than the document cycle
// in which its display items are created during painting.
// After the document cycle, a pointer/reference to DisplayItemClient should be
// no longer dereferenced unless we can make sure the client is still valid.
class PLATFORM_EXPORT DisplayItemClient {
public:
#if ENABLE(ASSERT)
    DisplayItemClient();
    virtual ~DisplayItemClient();
#else
    virtual ~DisplayItemClient() { }
#endif

    virtual String debugName() const = 0;

    // The visual rect of this DisplayItemClient, in object space of the object that owns the GraphicsLayer, i.e.
    // offset by offsetFromLayoutObjectWithSubpixelAccumulation().
    virtual LayoutRect visualRect() const = 0;

    virtual bool displayItemsAreCached(DisplayItemCacheGeneration) const = 0;
    virtual void setDisplayItemsCached(DisplayItemCacheGeneration) const = 0;
    virtual void setDisplayItemsUncached() const = 0;

#if ENABLE(ASSERT)
    // Tests if a DisplayItemClient object has been created and has not been deleted yet.
    static bool isAlive(const DisplayItemClient&);
#endif

protected:
};

#define DISPLAY_ITEM_CACHE_STATUS_IMPLEMENTATION \
    bool displayItemsAreCached(DisplayItemCacheGeneration cacheGeneration) const final { return m_cacheGeneration.matches(cacheGeneration); } \
    void setDisplayItemsCached(DisplayItemCacheGeneration cacheGeneration) const final { m_cacheGeneration = cacheGeneration; } \
    void setDisplayItemsUncached() const final { m_cacheGeneration.invalidate(); } \
    mutable DisplayItemCacheGeneration m_cacheGeneration;

#define DISPLAY_ITEM_CACHE_STATUS_UNCACHEABLE_IMPLEMENTATION \
    bool displayItemsAreCached(DisplayItemCacheGeneration) const final { return false; } \
    void setDisplayItemsCached(DisplayItemCacheGeneration) const final { } \
    void setDisplayItemsUncached() const final { }

inline bool operator==(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 == &client2; }
inline bool operator!=(const DisplayItemClient& client1, const DisplayItemClient& client2) { return &client1 != &client2; }

} // namespace blink

#endif // DisplayItemClient_h
