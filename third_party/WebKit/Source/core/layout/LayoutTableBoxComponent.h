// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTableBoxComponent_h
#define LayoutTableBoxComponent_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"

namespace blink {

class LayoutTable;

// Common super class for LayoutTableCol, LayoutTableSection and LayoutTableRow.
class CORE_EXPORT LayoutTableBoxComponent : public LayoutBox {
public:
    bool backgroundChangedSinceLastPaintInvalidation() const { return m_backgroundChangedSinceLastPaintInvalidation; }
    void clearBackgroundChangedSinceLastPaintInvalidation() { m_backgroundChangedSinceLastPaintInvalidation = false; }
    static bool doCellsHaveDirtyWidth(const LayoutObject& tablePart, const LayoutTable&, const StyleDifference&, const ComputedStyle& oldStyle);
protected:
    explicit LayoutTableBoxComponent(Element* element)
        : LayoutBox(element)
        , m_backgroundChangedSinceLastPaintInvalidation(false)
    {
    }

    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    LayoutObject* firstChild() const { DCHECK(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { DCHECK(children() == virtualChildren()); return children()->lastChild(); }

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    void clearPaintInvalidationFlags() override
    {
        LayoutBox::clearPaintInvalidationFlags();
        m_backgroundChangedSinceLastPaintInvalidation = false;
    }

#if ENABLE(ASSERT)
    bool paintInvalidationStateIsDirty() const override { return m_backgroundChangedSinceLastPaintInvalidation || LayoutBox::paintInvalidationStateIsDirty(); }
#endif

private:
    // If you have a LayoutTableBoxComponent, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    LayoutObjectChildList* virtualChildren() override { return children(); }
    const LayoutObjectChildList* virtualChildren() const override { return children(); }

    LayoutObjectChildList m_children;
    bool m_backgroundChangedSinceLastPaintInvalidation;
};

} // namespace blink

#endif // LayoutTableBoxComponent_h
