// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransparencyDisplayItem_h
#define TransparencyDisplayItem_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/PassOwnPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginTransparencyDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<BeginTransparencyDisplayItem> create(DisplayItemClient client, Type type, const CompositeOperator preTransparencyLayerCompositeOp, const WebBlendMode& preTransparencyLayerBlendMode, const float opacity, const CompositeOperator postTransparencyLayerCompositeOp) { return adoptPtr(new BeginTransparencyDisplayItem(client, type, preTransparencyLayerCompositeOp, preTransparencyLayerBlendMode, opacity, postTransparencyLayerCompositeOp)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "BeginTransparency"; }
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

protected:
    BeginTransparencyDisplayItem(DisplayItemClient client, Type type, const CompositeOperator preTransparencyLayerCompositeOp, const WebBlendMode& preTransparencyLayerBlendMode, const float opacity, const CompositeOperator postTransparencyLayerCompositeOp)
        : DisplayItem(client, type)
        , m_preTransparencyLayerCompositeOp(preTransparencyLayerCompositeOp)
        , m_preTransparencyLayerBlendMode(preTransparencyLayerBlendMode)
        , m_opacity(opacity)
        , m_postTransparencyLayerCompositeOp(postTransparencyLayerCompositeOp) { }

private:
    const CompositeOperator m_preTransparencyLayerCompositeOp;
    const WebBlendMode m_preTransparencyLayerBlendMode;
    const float m_opacity;
    const CompositeOperator m_postTransparencyLayerCompositeOp;
};

class PLATFORM_EXPORT EndTransparencyDisplayItem : public DisplayItem {
public:
    static PassOwnPtr<EndTransparencyDisplayItem> create(DisplayItemClient client, Type type) { return adoptPtr(new EndTransparencyDisplayItem(client, type)); }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

protected:
    EndTransparencyDisplayItem(DisplayItemClient client, Type type)
        : DisplayItem(client, type) { }

private:
#ifndef NDEBUG
    virtual const char* name() const override { return "EndTransparency"; }
#endif
};

} // namespace blink

#endif // TransparencyDisplayItem_h
