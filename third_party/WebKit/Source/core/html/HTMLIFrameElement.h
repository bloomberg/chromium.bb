/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLIFrameElement_h
#define HTMLIFrameElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLIFrameElementSandbox.h"
#include "platform/Supplementable.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebVector.h"

namespace blink {

class CORE_EXPORT HTMLIFrameElement final
    : public HTMLFrameElementBase,
      public Supplementable<HTMLIFrameElement> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElement);

 public:
  DECLARE_NODE_FACTORY(HTMLIFrameElement);
  DECLARE_VIRTUAL_TRACE();
  ~HTMLIFrameElement() override;
  DOMTokenList* sandbox() const;

  Vector<WebParsedFeaturePolicyDeclaration> ConstructContainerPolicy(
      Vector<String>* /* messages */,
      bool* /* old_syntax */) const override;

 private:
  explicit HTMLIFrameElement(Document&);

  void SetCollapsed(bool) override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  bool LayoutObjectIsNeeded(const ComputedStyle&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool LoadedNonEmptyDocument() const override {
    return did_load_non_empty_document_;
  }
  void DidLoadNonEmptyDocument() override {
    did_load_non_empty_document_ = true;
  }
  bool IsInteractiveContent() const override;

  ReferrerPolicy ReferrerPolicyAttribute() override;

  // FrameOwner overrides:
  bool AllowFullscreen() const override { return allow_fullscreen_; }
  bool AllowPaymentRequest() const override { return allow_payment_request_; }
  AtomicString Csp() const override { return csp_; }

  AtomicString name_;
  AtomicString csp_;
  AtomicString allow_;
  bool did_load_non_empty_document_;
  bool allow_fullscreen_;
  bool allow_payment_request_;
  bool collapsed_by_client_;
  Member<HTMLIFrameElementSandbox> sandbox_;

  ReferrerPolicy referrer_policy_;
};

}  // namespace blink

#endif  // HTMLIFrameElement_h
