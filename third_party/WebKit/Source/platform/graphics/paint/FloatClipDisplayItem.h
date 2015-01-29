// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipDisplayItem_h
#define FloatClipDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class RoundedRect;

class PLATFORM_EXPORT FloatClipDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<FloatClipDisplayItem> create(DisplayItemClient client, Type type, const FloatRect& clipRect)
    {
        return adoptPtr(new FloatClipDisplayItem(client, type, clipRect));
    }

    FloatClipDisplayItem(DisplayItemClient client, Type type, const FloatRect& clipRect)
        : DisplayItem(client, type)
        , m_clipRect(clipRect)
    {
        ASSERT(isFloatClipType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    FloatRect m_clipRect;
#ifndef NDEBUG
    virtual const char* name() const override { return "FloatClip"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
};

class PLATFORM_EXPORT EndFloatClipDisplayItem : public DisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndFloatClipDisplayItem> create(DisplayItemClient client, Type type)
    {
        return adoptPtr(new EndFloatClipDisplayItem(client, type));
    }

    EndFloatClipDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type)
    {
        ASSERT(isEndFloatClipType(type));
    }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndFloatClip"; }
#endif
};

} // namespace blink

#endif // FloatClipDisplayItem_h
