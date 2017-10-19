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
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

bool CheckEmptyToken(const String& token, ExceptionState& exception_state) {
  if (!token.IsEmpty())
    return true;
  exception_state.ThrowDOMException(kSyntaxError,
                                    "The token provided must not be empty.");
  return false;
}

bool CheckTokenWithWhitespace(const String& token,
                              ExceptionState& exception_state) {
  if (token.Find(IsHTMLSpace) == kNotFound)
    return true;
  exception_state.ThrowDOMException(kInvalidCharacterError,
                                    "The token provided ('" + token +
                                        "') contains HTML space characters, "
                                        "which are not valid in tokens.");
  return false;
}

// This implements the common part of the following operations:
// https://dom.spec.whatwg.org/#dom-domtokenlist-add
// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
// https://dom.spec.whatwg.org/#dom-domtokenlist-toggle
// https://dom.spec.whatwg.org/#dom-domtokenlist-replace
bool CheckTokenSyntax(const String& token, ExceptionState& exception_state) {
  // 1. If token is the empty string, then throw a SyntaxError.
  if (!CheckEmptyToken(token, exception_state))
    return false;

  // 2. If token contains any ASCII whitespace, then throw an
  // InvalidCharacterError.
  return CheckTokenWithWhitespace(token, exception_state);
}

bool CheckTokensSyntax(const Vector<String>& tokens,
                       ExceptionState& exception_state) {
  for (const auto& token : tokens) {
    if (!CheckTokenSyntax(token, exception_state))
      return false;
  }
  return true;
}

}  // anonymous namespace

void DOMTokenList::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
}

// https://dom.spec.whatwg.org/#concept-domtokenlist-validation
bool DOMTokenList::ValidateTokenValue(const AtomicString&,
                                      ExceptionState& exception_state) const {
  exception_state.ThrowTypeError("DOMTokenList has no supported tokens.");
  return false;
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-contains
bool DOMTokenList::contains(const AtomicString& token) const {
  return token_set_.Contains(token);
}

void DOMTokenList::Add(const AtomicString& token) {
  add(Vector<String>({token}), ASSERT_NO_EXCEPTION);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-add
// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::add(const Vector<String>& tokens,
                       ExceptionState& exception_state) {
  if (!CheckTokensSyntax(tokens, exception_state))
    return;
  AddTokens(tokens);
}

void DOMTokenList::Remove(const AtomicString& token) {
  remove(Vector<String>({token}), ASSERT_NO_EXCEPTION);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::remove(const Vector<String>& tokens,
                          ExceptionState& exception_state) {
  if (!CheckTokensSyntax(tokens, exception_state))
    return;

  // TODO(tkent): This null check doesn't conform to the DOM specification.
  // See https://github.com/whatwg/dom/issues/462
  if (value().IsNull())
    return;
  RemoveTokens(tokens);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-toggle
bool DOMTokenList::toggle(const AtomicString& token,
                          ExceptionState& exception_state) {
  if (!CheckTokenSyntax(token, exception_state))
    return false;

  // 4. If context object’s token set[token] exists, then:
  if (contains(token)) {
    // 1. If force is either not given or is false, then remove token from
    // context object’s token set.
    RemoveTokens(Vector<String>({token}));
    return false;
  }
  // 5. Otherwise, if force not given or is true, append token to context
  // object’s token set and set result to true.
  AddTokens(Vector<String>({token}));
  return true;
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-toggle
bool DOMTokenList::toggle(const AtomicString& token,
                          bool force,
                          ExceptionState& exception_state) {
  if (!CheckTokenSyntax(token, exception_state))
    return false;

  // 4. If context object’s token set[token] exists, then:
  if (contains(token)) {
    // 1. If force is either not given or is false, then remove token from
    // context object’s token set.
    if (!force)
      RemoveTokens(Vector<String>({token}));
  } else {
    // 5. Otherwise, if force not given or is true, append token to context
    // object’s token set and set result to true.
    if (force)
      AddTokens(Vector<String>({token}));
  }

  return force;
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-replace
void DOMTokenList::replace(const AtomicString& token,
                           const AtomicString& new_token,
                           ExceptionState& exception_state) {
  // 1. If either token or newToken is the empty string, then throw a
  // SyntaxError.
  if (!CheckEmptyToken(token, exception_state) ||
      !CheckEmptyToken(new_token, exception_state))
    return;

  // 2. If either token or newToken contains any ASCII whitespace, then throw an
  // InvalidCharacterError.
  if (!CheckTokenWithWhitespace(token, exception_state) ||
      !CheckTokenWithWhitespace(new_token, exception_state))
    return;

  // https://infra.spec.whatwg.org/#set-replace
  // To replace within an ordered set set, given item and replacement: if set
  // contains item or replacement, then replace the first instance of either
  // with replacement and remove all other instances.
  bool found_old_token = false;
  bool found_new_token = false;
  bool did_update = false;
  for (size_t i = 0; i < token_set_.size(); ++i) {
    const AtomicString& existing_token = token_set_[i];
    if (found_old_token) {
      if (existing_token == new_token) {
        token_set_.Remove(i);
        break;
      }
    } else if (found_new_token) {
      if (existing_token == token) {
        token_set_.Remove(i);
        did_update = true;
        break;
      }
    } else if (existing_token == token) {
      found_old_token = true;
      token_set_.ReplaceAt(i, new_token);
      did_update = true;
    } else if (existing_token == new_token) {
      found_new_token = true;
    }
  }

  // TODO(tkent): This check doesn't conform to the DOM specification, but it's
  // interoperable with Firefox and Safari.
  // https://github.com/whatwg/dom/issues/462
  if (!did_update)
    return;

  UpdateWithTokenSet(token_set_);
}

bool DOMTokenList::supports(const AtomicString& token,
                            ExceptionState& exception_state) {
  return ValidateTokenValue(token, exception_state);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-add
void DOMTokenList::AddTokens(const Vector<String>& tokens) {
  // 2. For each token in tokens, append token to context object’s token set.
  for (const auto& token : tokens)
    token_set_.Add(AtomicString(token));
  // 3. Run the update steps.
  UpdateWithTokenSet(token_set_);
}

// https://dom.spec.whatwg.org/#dom-domtokenlist-remove
void DOMTokenList::RemoveTokens(const Vector<String>& tokens) {
  // 2. For each token in tokens, remove token from context object’s token set.
  for (const auto& token : tokens)
    token_set_.Remove(AtomicString(token));
  // 3. Run the update steps.
  UpdateWithTokenSet(token_set_);
}

// https://dom.spec.whatwg.org/#concept-ordered-set-serializer
// The ordered set serializer takes a set and returns the concatenation of the
// strings in set, separated from each other by U+0020, if set is non-empty, and
// the empty string otherwise.
AtomicString DOMTokenList::SerializeTokenSet(
    const SpaceSplitString& token_set) {
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

// https://dom.spec.whatwg.org/#concept-dtl-update
void DOMTokenList::UpdateWithTokenSet(const SpaceSplitString& token_set) {
  AutoReset<bool> updating(&is_in_update_step_, true);
  setValue(SerializeTokenSet(token_set));
}

void DOMTokenList::setValue(const AtomicString& value) {
  element_->setAttribute(attribute_name_, value);
}

void DOMTokenList::DidUpdateAttributeValue(const AtomicString& old_value,
                                           const AtomicString& new_value) {
  value_ = new_value;
  if (is_in_update_step_)
    return;
  if (old_value != new_value)
    token_set_.Set(new_value);
}

const AtomicString DOMTokenList::item(unsigned index) const {
  if (index >= length())
    return AtomicString();
  return token_set_[index];
}

}  // namespace blink
