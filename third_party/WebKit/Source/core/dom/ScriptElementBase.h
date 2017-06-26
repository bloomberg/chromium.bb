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
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {
class Document;
class Element;
class HTMLScriptElementOrSVGScriptElement;
class ScriptLoader;

class CORE_EXPORT ScriptElementBase : public GarbageCollectedMixin {
 public:
  static ScriptElementBase* FromElementIfPossible(Element*);

  virtual void DispatchLoadEvent() = 0;
  virtual void DispatchErrorEvent() = 0;

  virtual bool AsyncAttributeValue() const = 0;
  virtual String CharsetAttributeValue() const = 0;
  virtual String CrossOriginAttributeValue() const = 0;
  virtual bool DeferAttributeValue() const = 0;
  virtual String EventAttributeValue() const = 0;
  virtual String ForAttributeValue() const = 0;
  virtual String IntegrityAttributeValue() const = 0;
  virtual String LanguageAttributeValue() const = 0;
  virtual bool NomoduleAttributeValue() const = 0;
  virtual String SourceAttributeValue() const = 0;
  virtual String TypeAttributeValue() const = 0;

  virtual String TextFromChildren() = 0;
  virtual String TextContent() const = 0;
  virtual bool HasSourceAttribute() const = 0;
  virtual bool IsConnected() const = 0;
  virtual bool HasChildren() const = 0;
  virtual const AtomicString& GetNonceForElement() const = 0;
  virtual AtomicString InitiatorName() const = 0;

  virtual bool AllowInlineScriptForCSP(const AtomicString& nonce,
                                       const WTF::OrdinalNumber&,
                                       const String& script_content) = 0;
  virtual Document& GetDocument() const = 0;
  virtual void SetScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) = 0;

  virtual ScriptLoader* Loader() const = 0;

 protected:
  ScriptLoader* InitializeScriptLoader(bool parser_inserted,
                                       bool already_started,
                                       bool created_during_document_write);
};

}  // namespace blink

#endif
