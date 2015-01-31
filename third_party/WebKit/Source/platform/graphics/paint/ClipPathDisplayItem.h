// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathDisplayItem_h
#define ClipPathDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginClipPathDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginClipPathDisplayItem> create(DisplayItemClient client, const Path& clipPath, WindRule windRule)
    {
        return adoptPtr(new BeginClipPathDisplayItem(client, clipPath, windRule));
    }

    BeginClipPathDisplayItem(DisplayItemClient client, const Path& clipPath, WindRule windRule)
        : DisplayItem(client, BeginClipPath)
        , m_clipPath(clipPath)
        , m_windRule(windRule) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    const Path m_clipPath;
    const WindRule m_windRule;
#ifndef NDEBUG
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
};

class PLATFORM_EXPORT EndClipPathDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndClipPathDisplayItem> create(DisplayItemClient client)
    {
        return adoptPtr(new EndClipPathDisplayItem(client));
    }

    EndClipPathDisplayItem(DisplayItemClient client)
        : DisplayItem(client, EndClipPath) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;
};

} // namespace blink

#endif // ClipPathDisplayItem_h
