// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectPaintPropertyNode_h
#define EffectPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// A paint effect created by the opacity css property along with a reference to
// the parent effect for inherited effects.
//
// The effect tree is rooted at a node with no parent. This root node should
// not be modified.
//
// TODO(pdr): Support more effects than just opacity.
class PLATFORM_EXPORT EffectPaintPropertyNode : public RefCounted<EffectPaintPropertyNode> {
public:
    static PassRefPtr<EffectPaintPropertyNode> create(PassRefPtr<const EffectPaintPropertyNode> parent, float opacity)
    {
        return adoptRef(new EffectPaintPropertyNode(std::move(parent), opacity));
    }

    void update(PassRefPtr<const EffectPaintPropertyNode> parent, float opacity)
    {
        DCHECK(!isRoot());
        DCHECK(parent != this);
        m_parent = parent;
        m_opacity = opacity;
    }

    float opacity() const { return m_opacity; }

    // Parent effect or nullptr if this is the root effect.
    const EffectPaintPropertyNode* parent() const { return m_parent.get(); }
    bool isRoot() const { return !m_parent; }

private:
    EffectPaintPropertyNode(PassRefPtr<const EffectPaintPropertyNode> parent, float opacity)
        : m_parent(parent), m_opacity(opacity) { }

    RefPtr<const EffectPaintPropertyNode> m_parent;
    float m_opacity;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);

} // namespace blink

#endif // EffectPaintPropertyNode_h
