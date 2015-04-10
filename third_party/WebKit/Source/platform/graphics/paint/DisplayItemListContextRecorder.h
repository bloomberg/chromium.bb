// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemListContextRecorder_h
#define DisplayItemListContextRecorder_h

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "wtf/OwnPtr.h"

namespace blink {

class DisplayItemListContextRecorder {
    WTF_MAKE_NONCOPYABLE(DisplayItemListContextRecorder);
    STACK_ALLOCATED();
public:
    DisplayItemListContextRecorder(GraphicsContext& context)
        : m_initialContext(context)
    {
        if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
            return;

        m_displayItemList = DisplayItemList::create();
        m_displayItemListContext = adoptPtr(new GraphicsContext(m_displayItemList.get(),
            context.contextDisabled() ? GraphicsContext::FullyDisabled : GraphicsContext::NothingDisabled));
    }

    ~DisplayItemListContextRecorder()
    {
        if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
            return;

        ASSERT(m_displayItemList);
        m_displayItemList->commitNewDisplayItemsAndReplay(m_initialContext);
    }

    GraphicsContext& context() const { return m_displayItemList ? *m_displayItemListContext : m_initialContext; }

private:
    GraphicsContext& m_initialContext;
    OwnPtr<DisplayItemList> m_displayItemList;
    OwnPtr<GraphicsContext> m_displayItemListContext;
};

} // namespace blink

#endif // DisplayItemListContextRecorder_h
