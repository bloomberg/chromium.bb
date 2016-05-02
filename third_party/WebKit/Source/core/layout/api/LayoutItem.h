// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutItem_h
#define LayoutItem_h

#include "core/layout/LayoutObject.h"

#include "wtf/Allocator.h"

namespace blink {

class FrameView;

class LayoutItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    explicit LayoutItem(LayoutObject* layoutObject)
        : m_layoutObject(layoutObject)
    {
    }

    LayoutItem(std::nullptr_t)
        : m_layoutObject(0)
    {
    }

    LayoutItem() : m_layoutObject(0) { }

    // TODO(leviw): This should be an UnspecifiedBoolType, but
    // using this operator allows the API to be landed in pieces.
    // https://crbug.com/499321
    operator LayoutObject*() const { return m_layoutObject; }

    // TODO(pilgrim): Remove this when we replace the operator above with UnspecifiedBoolType.
    bool isNull() const
    {
        return !m_layoutObject;
    }

    bool isDescendantOf(LayoutItem item) const
    {
        return m_layoutObject->isDescendantOf(item.layoutObject());
    }

    bool isBoxModelObject() const
    {
        return m_layoutObject->isBoxModelObject();
    }

    bool isBox() const
    {
        return m_layoutObject->isBox();
    }

    bool isBR() const
    {
        return m_layoutObject->isBR();
    }

    bool isLayoutBlock() const
    {
        return m_layoutObject->isLayoutBlock();
    }

    bool isText() const
    {
        return m_layoutObject->isText();
    }

    bool isTextControl() const
    {
        return m_layoutObject->isTextControl();
    }

    bool isLayoutPart() const
    {
        return m_layoutObject->isLayoutPart();
    }

    bool isEmbeddedObject() const
    {
        return m_layoutObject->isEmbeddedObject();
    }

    bool isImage() const
    {
        return m_layoutObject->isImage();
    }

    bool isLayoutFullScreen() const
    {
        return m_layoutObject->isLayoutFullScreen();
    }

    bool isListItem() const
    {
        return m_layoutObject->isListItem();
    }

    bool isMedia() const
    {
        return m_layoutObject->isMedia();
    }

    bool isMenuList() const
    {
        return m_layoutObject->isMenuList();
    }

    bool isProgress() const
    {
        return m_layoutObject->isProgress();
    }

    bool isSlider() const
    {
        return m_layoutObject->isSlider();
    }

    bool isLayoutView() const
    {
        return m_layoutObject->isLayoutView();
    }

    bool needsLayout()
    {
        return m_layoutObject->needsLayout();
    }

    void layout()
    {
        m_layoutObject->layout();
    }

    LayoutItem container() const
    {
        return LayoutItem(m_layoutObject->container());
    }

    const ComputedStyle& styleRef() const
    {
        return m_layoutObject->styleRef();
    }

    LayoutSize offsetFromContainer(const LayoutItem& item) const
    {
        return m_layoutObject->offsetFromContainer(item.layoutObject());
    }

    FrameView* frameView() const
    {
        return m_layoutObject->document().view();
    }

    void setMayNeedPaintInvalidation()
    {
        m_layoutObject->setMayNeedPaintInvalidation();
    }

    const ComputedStyle* style() const
    {
        return m_layoutObject->style();
    }

    bool hasLayer() const
    {
        return m_layoutObject->hasLayer();
    }

    void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants()
    {
        m_layoutObject->setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
    }

    void computeLayerHitTestRects(LayerHitTestRects& layerRects) const
    {
        m_layoutObject->computeLayerHitTestRects(layerRects);
    }

    FloatPoint absoluteToLocal(const FloatPoint& point, MapCoordinatesFlags mode = 0) const
    {
        return m_layoutObject->absoluteToLocal(point, mode);
    }

protected:
    LayoutObject* layoutObject() { return m_layoutObject; }
    const LayoutObject* layoutObject() const { return m_layoutObject; }

private:
    LayoutObject* m_layoutObject;
};

} // namespace blink

#endif // LayoutItem_h
