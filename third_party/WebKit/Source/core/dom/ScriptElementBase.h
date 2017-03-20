/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#ifndef ScriptElementBase_h
#define ScriptElementBase_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace blink {
class Document;
class Element;
class HTMLScriptElementOrSVGScriptElement;
class ScriptLoader;

class CORE_EXPORT ScriptElementBase : public GarbageCollectedMixin {
 public:
  virtual ~ScriptElementBase() {}

  static ScriptElementBase* fromElementIfPossible(Element*);

  virtual void dispatchLoadEvent() = 0;
  virtual void dispatchErrorEvent() = 0;

  virtual bool asyncAttributeValue() const = 0;
  virtual String charsetAttributeValue() const = 0;
  virtual String crossOriginAttributeValue() const = 0;
  virtual bool deferAttributeValue() const = 0;
  virtual String eventAttributeValue() const = 0;
  virtual String forAttributeValue() const = 0;
  virtual String integrityAttributeValue() const = 0;
  virtual String languageAttributeValue() const = 0;
  virtual String sourceAttributeValue() const = 0;
  virtual String typeAttributeValue() const = 0;

  virtual String textFromChildren() = 0;
  virtual String textContent() const = 0;
  virtual bool hasSourceAttribute() const = 0;
  virtual bool isConnected() const = 0;
  virtual bool hasChildren() const = 0;
  virtual bool isNonceableElement() const = 0;
  virtual AtomicString initiatorName() const = 0;

  virtual bool allowInlineScriptForCSP(const AtomicString& nonce,
                                       const WTF::OrdinalNumber&,
                                       const String& scriptContent) = 0;
  virtual Document& document() const = 0;
  virtual void setScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) = 0;

  ScriptLoader* loader() const { return m_loader.get(); }

  AtomicString nonce() const { return m_nonce; }
  void setNonce(const String& nonce) { m_nonce = AtomicString(nonce); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  void initializeScriptLoader(bool parserInserted,
                              bool alreadyStarted,
                              bool createdDuringDocumentWrite);

  Member<ScriptLoader> m_loader;

 private:
  AtomicString m_nonce;
};

}  // namespace blink

#endif
