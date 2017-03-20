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
  static HTMLScriptElement* create(Document&,
                                   bool wasInsertedByParser,
                                   bool alreadyStarted = false,
                                   bool createdDuringDocumentWrite = false);

  String text() { return textFromChildren(); }
  void setText(const String&);

  KURL src() const;

  void setAsync(bool);
  bool async() const;

  ScriptLoader* loader() const { return m_loader.get(); }

  bool isScriptElement() const override { return true; }
  Document& document() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLScriptElement(Document&,
                    bool wasInsertedByParser,
                    bool alreadyStarted,
                    bool createdDuringDocumentWrite);

  void parseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void didNotifySubtreeInsertionsToDocument() override;
  void childrenChanged(const ChildrenChange&) override;
  void didMoveToNewDocument(Document& oldDocument) override;

  bool isURLAttribute(const Attribute&) const override;
  bool hasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& subResourceAttributeName() const override;

  // ScriptElementBase overrides:
  String sourceAttributeValue() const override;
  String charsetAttributeValue() const override;
  String typeAttributeValue() const override;
  String languageAttributeValue() const override;
  String forAttributeValue() const override;
  String eventAttributeValue() const override;
  String crossOriginAttributeValue() const override;
  String integrityAttributeValue() const override;
  String textFromChildren() override;
  String textContent() const override;
  bool asyncAttributeValue() const override;
  bool deferAttributeValue() const override;
  bool hasSourceAttribute() const override;
  bool isConnected() const override;
  bool hasChildren() const override;
  bool isNonceableElement() const override;
  bool allowInlineScriptForCSP(const AtomicString& nonce,
                               const WTF::OrdinalNumber&,
                               const String& scriptContent) override;
  AtomicString initiatorName() const override;
  void dispatchLoadEvent() override;
  void dispatchErrorEvent() override;
  void setScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) override;

  Element* cloneElementWithoutAttributesAndChildren() override;
};

}  // namespace blink

#endif  // HTMLScriptElement_h
