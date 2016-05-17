// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTableBoxComponent_h
#define LayoutTableBoxComponent_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"

namespace blink {

// Common super class for LayoutTableCol, LayoutTableSection and LayoutTableRow.
class CORE_EXPORT LayoutTableBoxComponent : public LayoutBox {
protected:
    explicit LayoutTableBoxComponent(Element* element)
        : LayoutBox(element)
    {
    }

    const LayoutObjectChildList* children() const { return &m_children; }
    LayoutObjectChildList* children() { return &m_children; }

    LayoutObject* firstChild() const { DCHECK(children() == virtualChildren()); return children()->firstChild(); }
    LayoutObject* lastChild() const { DCHECK(children() == virtualChildren()); return children()->lastChild(); }

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

private:
    // If you have a LayoutTableBoxComponent, use firstChild or lastChild instead.
    void slowFirstChild() const = delete;
    void slowLastChild() const = delete;

    LayoutObjectChildList* virtualChildren() override { return children(); }
    const LayoutObjectChildList* virtualChildren() const override { return children(); }

    LayoutObjectChildList m_children;
};

} // namespace blink

#endif // LayoutTableBoxComponent_h
