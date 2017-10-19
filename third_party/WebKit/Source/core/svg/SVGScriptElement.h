/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGScriptElement_h
#define SVGScriptElement_h

#include "core/dom/ScriptElementBase.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg_names.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptLoader;

class SVGScriptElement final : public SVGElement,
                               public SVGURIReference,
                               public ScriptElementBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGScriptElement);

 public:
  static SVGScriptElement* Create(Document&, bool was_inserted_by_parser);

  ScriptLoader* Loader() const final { return loader_.Get(); }

#if DCHECK_IS_ON()
  bool IsAnimatableAttribute(const QualifiedName&) const override;
#endif

  bool IsScriptElement() const override { return true; }

  virtual void Trace(blink::Visitor*);
  DECLARE_TRACE_WRAPPERS();

 private:
  SVGScriptElement(Document&,
                   bool was_inserted_by_parser,
                   bool already_started);

  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void ChildrenChanged(const ChildrenChange&) override;
  void DidMoveToNewDocument(Document& old_document) override;

  void SvgAttributeChanged(const QualifiedName&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool IsStructurallyExternal() const override { return HasSourceAttribute(); }
  void FinishParsingChildren() override;

  bool HaveLoadedRequiredResources() override;

  // ScriptElementBase overrides:
  bool AsyncAttributeValue() const { return false; }
  String CharsetAttributeValue() const { return String(); }
  String CrossOriginAttributeValue() const { return String(); }
  bool DeferAttributeValue() const { return false; }
  String EventAttributeValue() const { return String(); }
  String ForAttributeValue() const { return String(); }
  String IntegrityAttributeValue() const { return String(); }
  String LanguageAttributeValue() const { return String(); }
  bool NomoduleAttributeValue() const { return false; }
  String SourceAttributeValue() const override;
  String TypeAttributeValue() const override;
  String TextFromChildren() override;
  bool HasSourceAttribute() const override;
  bool IsConnected() const override;
  bool HasChildren() const override;
  const AtomicString& GetNonceForElement() const override;
  bool AllowInlineScriptForCSP(const AtomicString& nonce,
                               const WTF::OrdinalNumber&,
                               const String& script_content,
                               ContentSecurityPolicy::InlineType) override;
  AtomicString InitiatorName() const override;
  Document& GetDocument() const override;
  void DispatchLoadEvent() override;
  void DispatchErrorEvent() override;
  void SetScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) override;

  Element* CloneElementWithoutAttributesAndChildren() override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) override { return false; }

  TraceWrapperMember<ScriptLoader> loader_;
};

}  // namespace blink

#endif  // SVGScriptElement_h
