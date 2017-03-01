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

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/frame/FrameOwner.h"
#include "core/html/HTMLElement.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/HashCountedSet.h"

namespace blink {

class ExceptionState;
class Frame;
class FrameViewBase;
class LayoutPart;

class CORE_EXPORT HTMLFrameOwnerElement : public HTMLElement,
                                          public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLFrameOwnerElement);

 public:
  ~HTMLFrameOwnerElement() override;

  DOMWindow* contentWindow() const;
  Document* contentDocument() const;

  virtual void disconnectContentFrame();

  // Most subclasses use LayoutPart (either LayoutEmbeddedObject or
  // LayoutIFrame) except for HTMLObjectElement and HTMLEmbedElement which may
  // return any LayoutObject when using fallback content.
  LayoutPart* layoutPart() const;

  Document* getSVGDocument(ExceptionState&) const;

  virtual bool loadedNonEmptyDocument() const { return false; }
  virtual void didLoadNonEmptyDocument() {}

  void setWidget(FrameViewBase*);
  FrameViewBase* releaseWidget();
  FrameViewBase* ownedWidget() const;

  class UpdateSuspendScope {
    STACK_ALLOCATED();

   public:
    UpdateSuspendScope();
    ~UpdateSuspendScope();

   private:
    void performDeferredWidgetTreeOperations();
  };

  // FrameOwner overrides:
  Frame* contentFrame() const final { return m_contentFrame; }
  void setContentFrame(Frame&) final;
  void clearContentFrame() final;
  void dispatchLoad() final;
  SandboxFlags getSandboxFlags() const final { return m_sandboxFlags; }
  bool canRenderFallbackContent() const override { return false; }
  void renderFallbackContent() override {}
  AtomicString browsingContextContainerName() const override {
    return getAttribute(HTMLNames::nameAttr);
  }
  ScrollbarMode scrollingMode() const override { return ScrollbarAuto; }
  int marginWidth() const override { return -1; }
  int marginHeight() const override { return -1; }
  bool allowFullscreen() const override { return false; }
  bool allowPaymentRequest() const override { return false; }
  AtomicString csp() const override { return nullAtom; }
  const WebVector<mojom::blink::PermissionName>& delegatedPermissions()
      const override;
  const WebVector<WebFeaturePolicyFeature>& allowedFeatures() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  HTMLFrameOwnerElement(const QualifiedName& tagName, Document&);
  void setSandboxFlags(SandboxFlags);

  bool loadOrRedirectSubframe(const KURL&,
                              const AtomicString& frameName,
                              bool replaceCurrentItem);
  bool isKeyboardFocusable() const override;

  void disposeWidgetSoon(FrameViewBase*);

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already HTMLFrameOwnerElement.
  bool isLocal() const final { return true; }
  bool isRemote() const final { return false; }

  bool isFrameOwnerElement() const final { return true; }

  virtual ReferrerPolicy referrerPolicyAttribute() {
    return ReferrerPolicyDefault;
  }

  Member<Frame> m_contentFrame;
  Member<FrameViewBase> m_widget;
  SandboxFlags m_sandboxFlags;
};

DEFINE_ELEMENT_TYPE_CASTS(HTMLFrameOwnerElement, isFrameOwnerElement());

class SubframeLoadingDisabler {
  STACK_ALLOCATED();

 public:
  explicit SubframeLoadingDisabler(Node& root)
      : SubframeLoadingDisabler(&root) {}

  explicit SubframeLoadingDisabler(Node* root) : m_root(root) {
    if (m_root)
      disabledSubtreeRoots().add(m_root);
  }

  ~SubframeLoadingDisabler() {
    if (m_root)
      disabledSubtreeRoots().remove(m_root);
  }

  static bool canLoadFrame(HTMLFrameOwnerElement& owner) {
    for (Node* node = &owner; node; node = node->parentOrShadowHostNode()) {
      if (disabledSubtreeRoots().contains(node))
        return false;
    }
    return true;
  }

 private:
  // The use of UntracedMember<Node>  is safe as all SubtreeRootSet
  // references are on the stack and reachable in case a conservative
  // GC hits.
  // TODO(sof): go back to HeapHashSet<> once crbug.com/684551 has been
  // resolved.
  using SubtreeRootSet = HashCountedSet<UntracedMember<Node>>;

  CORE_EXPORT static SubtreeRootSet& disabledSubtreeRoots();

  Member<Node> m_root;
};

DEFINE_TYPE_CASTS(HTMLFrameOwnerElement,
                  FrameOwner,
                  owner,
                  owner->isLocal(),
                  owner.isLocal());

}  // namespace blink

#endif  // HTMLFrameOwnerElement_h
