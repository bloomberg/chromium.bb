/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLDocument_h
#define HTMLDocument_h

#include "core/dom/Document.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/wtf/HashCountedSet.h"

namespace blink {

class HTMLBodyElement;

class CORE_EXPORT HTMLDocument : public Document {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLDocument* Create(const DocumentInit& initializer) {
    return new HTMLDocument(initializer);
  }
  static HTMLDocument* CreateForTest() {
    return new HTMLDocument(DocumentInit::Create());
  }
  ~HTMLDocument() override;

  const AtomicString& bgColor() const;
  void setBgColor(const AtomicString&);
  const AtomicString& fgColor() const;
  void setFgColor(const AtomicString&);
  const AtomicString& alinkColor() const;
  void setAlinkColor(const AtomicString&);
  const AtomicString& linkColor() const;
  void setLinkColor(const AtomicString&);
  const AtomicString& vlinkColor() const;
  void setVlinkColor(const AtomicString&);

  void clear() {}

  void captureEvents() {}
  void releaseEvents() {}

  void AddNamedItem(const AtomicString& name);
  void RemoveNamedItem(const AtomicString& name);
  bool HasNamedItem(const AtomicString& name);

  void AddExtraNamedItem(const AtomicString& name);
  void RemoveExtraNamedItem(const AtomicString& name);
  bool HasExtraNamedItem(const AtomicString& name);

  static bool IsCaseSensitiveAttribute(const QualifiedName&);

  Document* CloneDocumentWithoutChildren() final;

 protected:
  HTMLDocument(
      const DocumentInit&,
      DocumentClassFlags extended_document_classes = kDefaultDocumentClass);

 private:
  HTMLBodyElement* HtmlBodyElement() const;

  const AtomicString& BodyAttributeValue(const QualifiedName&) const;
  void SetBodyAttribute(const QualifiedName&, const AtomicString&);

  void AddItemToMap(HashCountedSet<AtomicString>&, const AtomicString&);
  void RemoveItemFromMap(HashCountedSet<AtomicString>&, const AtomicString&);

  HashCountedSet<AtomicString> named_item_counts_;
  HashCountedSet<AtomicString> extra_named_item_counts_;
};

inline bool HTMLDocument::HasNamedItem(const AtomicString& name) {
  return named_item_counts_.Contains(name);
}

inline bool HTMLDocument::HasExtraNamedItem(const AtomicString& name) {
  return extra_named_item_counts_.Contains(name);
}

DEFINE_DOCUMENT_TYPE_CASTS(HTMLDocument);

}  // namespace blink

#endif  // HTMLDocument_h
