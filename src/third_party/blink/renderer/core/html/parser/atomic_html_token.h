/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_

#include <memory>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html_element_lookup_trie.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT AtomicHTMLToken {
  STACK_ALLOCATED();

 public:
  bool ForceQuirks() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->force_quirks_;
  }

  HTMLToken::TokenType GetType() const { return type_; }

  const AtomicString& GetName() const {
    DCHECK(UsesName());
    return name_;
  }

  void SetName(const AtomicString& name) {
    DCHECK(UsesName());
    name_ = name;
  }

  bool SelfClosing() const {
    DCHECK(type_ == HTMLToken::kStartTag || type_ == HTMLToken::kEndTag);
    return self_closing_;
  }

  bool HasDuplicateAttribute() const { return duplicate_attribute_; }

  Attribute* GetAttributeItem(const QualifiedName& attribute_name) {
    DCHECK(UsesAttributes());
    return FindAttributeInVector(attributes_, attribute_name);
  }

  Vector<Attribute>& Attributes() {
    DCHECK(UsesAttributes());
    return attributes_;
  }

  const Vector<Attribute>& Attributes() const {
    DCHECK(UsesAttributes());
    return attributes_;
  }

  const String& Characters() const {
    DCHECK_EQ(type_, HTMLToken::kCharacter);
    return data_;
  }

  const String& Comment() const {
    DCHECK_EQ(type_, HTMLToken::kComment);
    return data_;
  }

  // FIXME: Distinguish between a missing public identifer and an empty one.
  Vector<UChar>& PublicIdentifier() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->public_identifier_;
  }

  // FIXME: Distinguish between a missing system identifer and an empty one.
  Vector<UChar>& SystemIdentifier() const {
    DCHECK_EQ(type_, HTMLToken::DOCTYPE);
    return doctype_data_->system_identifier_;
  }

  explicit AtomicHTMLToken(HTMLToken& token) : type_(token.GetType()) {
    switch (type_) {
      case HTMLToken::kUninitialized:
        NOTREACHED();
        break;
      case HTMLToken::DOCTYPE:
        name_ = token.GetName().AsAtomicString();
        doctype_data_ = token.ReleaseDoctypeData();
        break;
      case HTMLToken::kEndOfFile:
        break;
      case HTMLToken::kStartTag:
      case HTMLToken::kEndTag: {
        self_closing_ = token.SelfClosing();
        if (const AtomicString& tag_name =
                lookupHTMLTag(token.GetName().data(), token.GetName().size()))
          name_ = tag_name;
        else
          name_ = token.GetName().AsAtomicString();
        InitializeAttributes(token.Attributes());
        break;
      }
      case HTMLToken::kCharacter:
      case HTMLToken::kComment:
        if (token.IsAll8BitData())
          data_ = token.Data().AsString8();
        else
          data_ = token.Data().AsString();
        break;
    }
  }

  explicit AtomicHTMLToken(HTMLToken::TokenType type) : type_(type) {}

  AtomicHTMLToken(HTMLToken::TokenType type,
                  const AtomicString& name,
                  const Vector<Attribute>& attributes = Vector<Attribute>())
      : type_(type),
        name_(name),
        attributes_(attributes) {
    DCHECK(UsesName());
  }

  AtomicHTMLToken(const AtomicHTMLToken&) = delete;
  AtomicHTMLToken& operator=(const AtomicHTMLToken&) = delete;

#ifndef NDEBUG
  void Show() const;
#endif

 private:
  HTMLToken::TokenType type_;

  void InitializeAttributes(const HTMLToken::AttributeList& attributes);
  QualifiedName NameForAttribute(const HTMLToken::Attribute&) const;

  bool UsesName() const;

  bool UsesAttributes() const;

  // "name" for DOCTYPE, StartTag, and EndTag
  AtomicString name_;

  // "data" for Comment, "characters" for Character
  String data_;

  // For DOCTYPE
  std::unique_ptr<DoctypeData> doctype_data_;

  // For StartTag and EndTag
  bool self_closing_ = false;

  bool duplicate_attribute_ = false;

  Vector<Attribute> attributes_;
};

inline void AtomicHTMLToken::InitializeAttributes(
    const HTMLToken::AttributeList& attributes) {
  wtf_size_t size = attributes.size();
  if (!size)
    return;

  attributes_.clear();
  attributes_.ReserveInitialCapacity(size);
  for (const auto& attribute : attributes) {
    if (attribute.NameIsEmpty())
      continue;

    attribute.NameRange().CheckValid();
    attribute.ValueRange().CheckValid();

    AtomicString value(attribute.GetValue());
    // The string pointer in |value| is null for attributes with no values, but
    // the null atom is used to represent absence of attributes; attributes with
    // no values have the value set to an empty atom instead.
    if (value == g_null_atom) {
      value = g_empty_atom;
    }
    const QualifiedName& name = NameForAttribute(attribute);
    // FIXME: This is N^2 for the number of attributes.
    if (!FindAttributeInVector(attributes_, name)) {
      attributes_.push_back(Attribute(name, value));
    } else {
      duplicate_attribute_ = true;
    }
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_HTML_TOKEN_H_
