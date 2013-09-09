/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLFrameOwnerElement_h
#define HTMLFrameOwnerElement_h

#include "core/html/HTMLElement.h"
#include "wtf/HashCountedSet.h"

namespace WebCore {

class DOMWindow;
class ExceptionState;
class Frame;
class RenderPart;
class SVGDocument;

class HTMLFrameOwnerElement : public HTMLElement {
public:
    virtual ~HTMLFrameOwnerElement();

    Frame* contentFrame() const { return m_contentFrame; }
    DOMWindow* contentWindow() const;
    Document* contentDocument() const;

    void setContentFrame(Frame*);
    void clearContentFrame();

    void disconnectContentFrame();

    // Most subclasses use RenderPart (either RenderEmbeddedObject or RenderIFrame)
    // except for HTMLObjectElement and HTMLEmbedElement which may return any
    // RenderObject when using fallback content.
    RenderPart* renderPart() const;

    SVGDocument* getSVGDocument(ExceptionState&) const;

    virtual ScrollbarMode scrollingMode() const { return ScrollbarAuto; }

    SandboxFlags sandboxFlags() const { return m_sandboxFlags; }

    virtual bool loadedNonEmptyDocument() const { return false; }
    virtual void didLoadNonEmptyDocument() { }

    virtual void renderFallbackContent() { }

    virtual bool isObjectElement() const { return false; }

protected:
    HTMLFrameOwnerElement(const QualifiedName& tagName, Document&);
    void setSandboxFlags(SandboxFlags);

    bool loadOrRedirectSubframe(const KURL&, const AtomicString& frameName, bool lockBackForwardList);

private:
    virtual bool isKeyboardFocusable() const OVERRIDE;
    virtual bool isFrameOwnerElement() const OVERRIDE { return true; }

    Frame* m_contentFrame;
    SandboxFlags m_sandboxFlags;
};

inline HTMLFrameOwnerElement* toHTMLFrameOwnerElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isFrameOwnerElement());
    return static_cast<HTMLFrameOwnerElement*>(node);
}

inline const HTMLFrameOwnerElement* toHTMLFrameOwnerElement(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isFrameOwnerElement());
    return static_cast<const HTMLFrameOwnerElement*>(node);
}

class SubframeLoadingDisabler {
public:
    explicit SubframeLoadingDisabler(Node* root)
        : m_root(root)
    {
        disabledSubtreeRoots().add(m_root);
    }

    ~SubframeLoadingDisabler()
    {
        disabledSubtreeRoots().remove(m_root);
    }

    static bool canLoadFrame(HTMLFrameOwnerElement* owner)
    {
        for (Node* node = owner; node; node = node->parentOrShadowHostNode()) {
            if (disabledSubtreeRoots().contains(node))
                return false;
        }
        return true;
    }

private:
    static HashCountedSet<Node*>& disabledSubtreeRoots()
    {
        DEFINE_STATIC_LOCAL(HashCountedSet<Node*>, nodes, ());
        return nodes;
    }

    Node* m_root;
};

} // namespace WebCore

#endif // HTMLFrameOwnerElement_h
