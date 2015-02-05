// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingDisplayItem_h
#define CompositingDisplayItem_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/PassOwnPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginCompositingDisplayItem : public PairedBeginDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<BeginCompositingDisplayItem> create(DisplayItemClient client, const CompositeOperator preCompositeOp, const WebBlendMode& preBlendMode, const float opacity, const CompositeOperator postCompositeOp)
    {
        return adoptPtr(new BeginCompositingDisplayItem(client, preCompositeOp, preBlendMode, opacity, postCompositeOp));
    }

    BeginCompositingDisplayItem(DisplayItemClient client, const CompositeOperator preCompositeOp, const WebBlendMode& preBlendMode, const float opacity, const CompositeOperator postCompositeOp)
        : PairedBeginDisplayItem(client, BeginCompositing)
        , m_preCompositeOp(preCompositeOp)
        , m_preBlendMode(preBlendMode)
        , m_opacity(opacity)
        , m_postCompositeOp(postCompositeOp) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    virtual void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
    const CompositeOperator m_preCompositeOp;
    const WebBlendMode m_preBlendMode;
    const float m_opacity;
    const CompositeOperator m_postCompositeOp;
};

class PLATFORM_EXPORT EndCompositingDisplayItem : public PairedEndDisplayItem {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<EndCompositingDisplayItem> create(DisplayItemClient client)
    {
        return adoptPtr(new EndCompositingDisplayItem(client));
    }

    EndCompositingDisplayItem(DisplayItemClient client)
        : PairedEndDisplayItem(client, EndCompositing) { }

    virtual void replay(GraphicsContext*) override;
    virtual void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    virtual bool isEndAndPairedWith(const DisplayItem& other) const override final { return other.type() == BeginCompositing; }
#endif
};

} // namespace blink

#endif // CompositingDisplayItem_h
