/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXScrollView_h
#define AXScrollView_h

#include "core/accessibility/AXObject.h"

namespace blink {

class AXScrollbar;
class Scrollbar;
class FrameView;

class AXScrollView final : public AXObject {
public:
    static PassRefPtr<AXScrollView> create(FrameView*);
    virtual AccessibilityRole roleValue() const override { return ScrollAreaRole; }
    FrameView* scrollView() const { return m_scrollView; }

    virtual ~AXScrollView();
    virtual void detach() override;

protected:
    virtual ScrollableArea* getScrollableAreaIfScrollable() const override;
    virtual void scrollTo(const IntPoint&) const override;

private:
    explicit AXScrollView(FrameView*);

    virtual bool computeAccessibilityIsIgnored() const override;
    virtual bool isAXScrollView() const override { return true; }
    virtual bool isEnabled() const override { return true; }

    virtual bool isAttachment() const override;
    virtual Widget* widgetForAttachmentView() const override;

    virtual AXObject* scrollBar(AccessibilityOrientation) override;
    virtual void addChildren() override;
    virtual void clearChildren() override;
    virtual AXObject* accessibilityHitTest(const IntPoint&) const override;
    virtual void updateChildrenIfNecessary() override;
    virtual void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    void updateScrollbars();

    virtual FrameView* documentFrameView() const override;
    virtual LayoutRect elementRect() const override;
    virtual AXObject* parentObject() const override;
    virtual AXObject* parentObjectIfExists() const override;

    AXObject* webAreaObject() const;
    virtual AXObject* firstChild() const override { return webAreaObject(); }
    AXScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AXObject*);

    // FIXME: Oilpan: Frame/ScrollView is on the heap and its
    // AXScrollView is detached&removed from the AX cache when the
    // FrameView is disposed. Which clears m_scrollView, hence this
    // bare pointer will not be stale, so the bare pointer use is safe
    // & acceptable.
    //
    // However, it would be preferable to have it be normally traced
    // as part of moving the AX objects to the heap. Temporarily using
    // a Persistent risks creating a FrameView leak, and brings no
    // real benefits overall.
    FrameView* m_scrollView;
    RefPtr<AXObject> m_horizontalScrollbar;
    RefPtr<AXObject> m_verticalScrollbar;
    bool m_childrenDirty;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXScrollView, isAXScrollView());

} // namespace blink

#endif // AXScrollView_h
