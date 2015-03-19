// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemListScope_h
#define DisplayItemListScope_h

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "wtf/OwnPtr.h"

namespace blink {

class DisplayItemListScope {
    WTF_MAKE_NONCOPYABLE(DisplayItemListScope);
    STACK_ALLOCATED();
public:
    DisplayItemListScope(GraphicsContext* context)
        : m_initialContext(context)
    {
        ASSERT(context);

        if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
            m_activeContext = context;
            return;
        }

        m_displayItemList = DisplayItemList::create();
        m_displayItemListContext = adoptPtr(new GraphicsContext(nullptr, m_displayItemList.get(),
            context->contextDisabled() ? GraphicsContext::FullyDisabled : GraphicsContext::NothingDisabled));
        m_activeContext = m_displayItemListContext.get();
    }

    ~DisplayItemListScope()
    {
        if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
            return;

        ASSERT(m_displayItemList);
        m_displayItemList->replay(m_initialContext);
    }

    GraphicsContext* context() const { return m_activeContext; }

private:
    GraphicsContext* m_initialContext;
    GraphicsContext* m_activeContext;
    OwnPtr<DisplayItemList> m_displayItemList;
    OwnPtr<GraphicsContext> m_displayItemListContext;
};

} // namespace blink

#endif // DisplayItemListScope_h
