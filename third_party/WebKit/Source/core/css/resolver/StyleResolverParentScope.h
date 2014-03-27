// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleResolverParentScope_h
#define StyleResolverParentScope_h

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"

namespace WebCore {

// Maintains the parent element stack (and bloom filter) inside recalcStyle.
class StyleResolverParentScope FINAL {
public:
    explicit StyleResolverParentScope(Element& parent);
    ~StyleResolverParentScope();

    static void ensureParentStackIsPushed();

private:
    void pushParentIfNeeded();

    Element& m_parent;
    bool m_pushed;
    StyleResolverParentScope* m_previous;

    static StyleResolverParentScope* s_currentScope;
};

inline StyleResolverParentScope::StyleResolverParentScope(Element& parent)
    : m_parent(parent)
    , m_pushed(false)
    , m_previous(s_currentScope)
{
    ASSERT(m_parent.document().inStyleRecalc());
    s_currentScope = this;
}

inline StyleResolverParentScope::~StyleResolverParentScope()
{
    if (m_pushed)
        m_parent.document().styleResolver()->popParentElement(m_parent);
    s_currentScope = m_previous;
}

inline void StyleResolverParentScope::ensureParentStackIsPushed()
{
    if (s_currentScope)
        s_currentScope->pushParentIfNeeded();
}

inline void StyleResolverParentScope::pushParentIfNeeded()
{
    if (m_pushed)
        return;
    if (m_previous)
        m_previous->pushParentIfNeeded();
    m_parent.document().styleResolver()->pushParentElement(m_parent);
    m_pushed = true;
}

} // namespace WebCore

#endif // StyleResolverParentScope_h
