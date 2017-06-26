/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef HTMLScriptElement_h
#define HTMLScriptElement_h

#include "core/CoreExport.h"
#include "core/dom/ScriptElementBase.h"
#include "core/dom/ScriptLoader.h"
#include "core/html/HTMLElement.h"

namespace blink {

class CORE_EXPORT HTMLScriptElement final : public HTMLElement,
                                            public ScriptElementBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLScriptElement);

 public:
  static HTMLScriptElement* Create(Document&,
                                   bool was_inserted_by_parser,
                                   bool already_started = false,
                                   bool created_during_document_write = false);

  String text() { return TextFromChildren(); }
  void setText(const String&);

  KURL Src() const;

  void setAsync(bool);
  bool async() const;

  ScriptLoader* Loader() const final { return loader_.Get(); }

  bool IsScriptElement() const override { return true; }
  Document& GetDocument() const override;

  DECLARE_VIRTUAL_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  HTMLScriptElement(Document&,
                    bool was_inserted_by_parser,
                    bool already_started,
                    bool created_during_document_write);

  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void ChildrenChanged(const ChildrenChange&) override;
  void DidMoveToNewDocument(Document& old_document) override;

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;

  // ScriptElementBase overrides:
  String SourceAttributeValue() const override;
  String CharsetAttributeValue() const override;
  String TypeAttributeValue() const override;
  String LanguageAttributeValue() const override;
  bool NomoduleAttributeValue() const override;
  String ForAttributeValue() const override;
  String EventAttributeValue() const override;
  String CrossOriginAttributeValue() const override;
  String IntegrityAttributeValue() const override;
  String TextFromChildren() override;
  String TextContent() const override;
  bool AsyncAttributeValue() const override;
  bool DeferAttributeValue() const override;
  bool HasSourceAttribute() const override;
  bool IsConnected() const override;
  bool HasChildren() const override;
  const AtomicString& GetNonceForElement() const override;
  bool AllowInlineScriptForCSP(const AtomicString& nonce,
                               const WTF::OrdinalNumber&,
                               const String& script_content) override;
  AtomicString InitiatorName() const override;
  void DispatchLoadEvent() override;
  void DispatchErrorEvent() override;
  void SetScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) override;

  Element* CloneElementWithoutAttributesAndChildren() override;

  TraceWrapperMember<ScriptLoader> loader_;
};

}  // namespace blink

#endif  // HTMLScriptElement_h
