/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/DOMTokenList.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

// This implements the common part of the following operations:
// https://dom.spec.whatwg.org/#dom-domtokenlist-add
// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
// https://dom.spec.whatwg.org/#dom-domtokenlist-toggle
// https://dom.spec.whatwg.org/#dom-domtokenlist-replace
bool DOMTokenList::ValidateToken(const String& token,
                                 ExceptionState& exception_state) const {
  // 1. If token is the empty string, then throw a SyntaxError.
  if (token.IsEmpty()) {
    exception_state.ThrowDOMException(kSyntaxError,
                                      "The token provided must not be empty.");
    return false;
  }

  // 2. If token contains any ASCII whitespace, then throw an
  // InvalidCharacterError.
  if (token.Find(IsHTMLSpace) != kNotFound) {
    exception_state.ThrowDOMException(kInvalidCharacterError,
                                      "The token provided ('" + token +
                                          "') contains HTML space characters, "
                                          "which are not valid in tokens.");
    return false;
  }

  return true;
}

bool DOMTokenList::ValidateTokens(const Vector<String>& tokens,
                                  ExceptionState& exception_state) const {
  for (const auto& token : tokens) {
    if (!ValidateToken(token, exception_state))
      return false;
  }

  return true;
}

// https://dom.spec.whatwg.org/#concept-domtokenlist-validation
bool DOMTokenList::ValidateTokenValue(const AtomicString&,
                                      ExceptionState& exception_state) const {
  exception_state.ThrowTypeError("DOMTokenList has no supported tokens.");
  return false;
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-contains
bool DOMTokenList::contains(const AtomicString& token) const {
  return ContainsInternal(token);
}

void DOMTokenList::add(const AtomicString& token,
                       ExceptionState& exception_state) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  add(tokens, exception_state);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-add
// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::add(const Vector<String>& tokens,
                       ExceptionState& exception_state) {
  if (!ValidateTokens(tokens, exception_state))
    return;

  setValue(AddTokens(tokens));
}

void DOMTokenList::remove(const AtomicString& token,
                          ExceptionState& exception_state) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  remove(tokens, exception_state);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::remove(const Vector<String>& tokens,
                          ExceptionState& exception_state) {
  if (!ValidateTokens(tokens, exception_state))
    return;

  // TODO(tkent): This null check doesn't conform to the DOM specification.
  // See https://github.com/whatwg/dom/issues/462
  if (value().IsNull())
    return;
  setValue(RemoveTokens(tokens));
}

bool DOMTokenList::toggle(const AtomicString& token,
                          ExceptionState& exception_state) {
  if (!ValidateToken(token, exception_state))
    return false;

  if (ContainsInternal(token)) {
    RemoveInternal(token);
    return false;
  }
  AddInternal(token);
  return true;
}

bool DOMTokenList::toggle(const AtomicString& token,
                          bool force,
                          ExceptionState& exception_state) {
  if (!ValidateToken(token, exception_state))
    return false;

  if (force)
    AddInternal(token);
  else
    RemoveInternal(token);

  return force;
}

bool DOMTokenList::supports(const AtomicString& token,
                            ExceptionState& exception_state) {
  return ValidateTokenValue(token, exception_state);
}

void DOMTokenList::AddInternal(const AtomicString& token) {
  if (!ContainsInternal(token))
    setValue(AddToken(token));
}

void DOMTokenList::RemoveInternal(const AtomicString& token) {
  // Check using contains first since it uses AtomicString comparisons instead
  // of character by character testing.
  if (!ContainsInternal(token))
    return;
  setValue(RemoveToken(token));
}

AtomicString DOMTokenList::AddToken(const AtomicString& token) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  return AddTokens(tokens);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-add
// This returns an AtomicString because it is always passed as argument to
// setValue() and setValue() takes an AtomicString in argument.
AtomicString DOMTokenList::AddTokens(const Vector<String>& tokens) {
  SpaceSplitString& token_set = MutableSet();
  // 2. For each token in tokens, append token to context object’s token set.
  for (const auto& token : tokens)
    token_set.Add(AtomicString(token));
  // 3. Run the update steps.
  return SerializeSet(token_set);
}

AtomicString DOMTokenList::RemoveToken(const AtomicString& token) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  return RemoveTokens(tokens);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
// This returns an AtomicString because it is always passed as argument to
// setValue() and setValue() takes an AtomicString in argument.
AtomicString DOMTokenList::RemoveTokens(const Vector<String>& tokens) {
  SpaceSplitString& token_set = MutableSet();
  // 2. For each token in tokens, remove token from context object’s token set.
  for (const auto& token : tokens)
    token_set.Remove(AtomicString(token));
  // 3. Run the update steps.
  return SerializeSet(token_set);
}

// https://dom.spec.whatwg.org/#concept-ordered-set-serializer
// The ordered set serializer takes a set and returns the concatenation of the
// strings in set, separated from each other by U+0020, if set is non-empty, and
// the empty string otherwise.
AtomicString DOMTokenList::SerializeSet(const SpaceSplitString& token_set) {
  size_t size = token_set.size();
  if (size == 0)
    return g_empty_atom;
  if (size == 1)
    return token_set[0];
  StringBuilder builder;
  builder.Append(token_set[0]);
  for (size_t i = 1; i < size; ++i) {
    builder.Append(' ');
    builder.Append(token_set[i]);
  }
  return builder.ToAtomicString();
}

void DOMTokenList::setValue(const AtomicString& value) {
  bool value_changed = value_ != value;
  value_ = value;
  if (value_changed)
    tokens_.Set(value);
  if (observer_)
    observer_->ValueWasSet();
}

bool DOMTokenList::ContainsInternal(const AtomicString& token) const {
  return tokens_.Contains(token);
}

const AtomicString DOMTokenList::item(unsigned index) const {
  if (index >= length())
    return AtomicString();
  return tokens_[index];
}

}  // namespace blink
