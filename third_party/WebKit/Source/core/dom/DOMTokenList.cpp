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

bool DOMTokenList::ValidateToken(const String& token,
                                 ExceptionState& exception_state) const {
  if (token.IsEmpty()) {
    exception_state.ThrowDOMException(kSyntaxError,
                                      "The token provided must not be empty.");
    return false;
  }

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

bool DOMTokenList::contains(const AtomicString& token,
                            ExceptionState& exception_state) const {
  if (!ValidateToken(token, exception_state))
    return false;
  return ContainsInternal(token);
}

void DOMTokenList::add(const AtomicString& token,
                       ExceptionState& exception_state) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  add(tokens, exception_state);
}

// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::add(const Vector<String>& tokens,
                       ExceptionState& exception_state) {
  Vector<String> filtered_tokens;
  filtered_tokens.ReserveCapacity(tokens.size());
  for (const auto& token : tokens) {
    if (!ValidateToken(token, exception_state))
      return;
    if (ContainsInternal(AtomicString(token)))
      continue;
    if (filtered_tokens.Contains(token))
      continue;
    filtered_tokens.push_back(token);
  }

  if (!filtered_tokens.IsEmpty())
    setValue(AddTokens(value(), filtered_tokens));
}

void DOMTokenList::remove(const AtomicString& token,
                          ExceptionState& exception_state) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  remove(tokens, exception_state);
}

// Optimally, this should take a Vector<AtomicString> const ref in argument but
// the bindings generator does not handle that.
void DOMTokenList::remove(const Vector<String>& tokens,
                          ExceptionState& exception_state) {
  if (!ValidateTokens(tokens, exception_state))
    return;

  // Check using containsInternal first since it is a lot faster than going
  // through the string character by character.
  bool found = false;
  for (const auto& token : tokens) {
    if (ContainsInternal(AtomicString(token))) {
      found = true;
      break;
    }
  }

  setValue(found ? RemoveTokens(value(), tokens) : value());
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
    setValue(AddToken(value(), token));
}

void DOMTokenList::RemoveInternal(const AtomicString& token) {
  // Check using contains first since it uses AtomicString comparisons instead
  // of character by character testing.
  if (!ContainsInternal(token))
    return;
  setValue(RemoveToken(value(), token));
}

AtomicString DOMTokenList::AddToken(const AtomicString& input,
                                    const AtomicString& token) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  return AddTokens(input, tokens);
}

// This returns an AtomicString because it is always passed as argument to
// setValue() and setValue() takes an AtomicString in argument.
AtomicString DOMTokenList::AddTokens(const AtomicString& input,
                                     const Vector<String>& tokens) {
  bool needs_space = false;

  StringBuilder builder;
  if (!input.IsEmpty()) {
    builder.Append(input);
    needs_space = !IsHTMLSpace<UChar>(input[input.length() - 1]);
  }

  for (const auto& token : tokens) {
    if (needs_space)
      builder.Append(' ');
    builder.Append(token);
    needs_space = true;
  }

  return builder.ToAtomicString();
}

AtomicString DOMTokenList::RemoveToken(const AtomicString& input,
                                       const AtomicString& token) {
  Vector<String> tokens;
  tokens.push_back(token.GetString());
  return RemoveTokens(input, tokens);
}

// This returns an AtomicString because it is always passed as argument to
// setValue() and setValue() takes an AtomicString in argument.
AtomicString DOMTokenList::RemoveTokens(const AtomicString& input,
                                        const Vector<String>& tokens) {
  // Algorithm defined at
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#remove-a-token-from-a-string
  // New spec is at https://dom.spec.whatwg.org/#remove-a-token-from-a-string

  unsigned input_length = input.length();
  StringBuilder output;  // 3
  output.ReserveCapacity(input_length);
  unsigned position = 0;  // 4

  // Step 5
  while (position < input_length) {
    if (IsHTMLSpace<UChar>(input[position])) {  // 6
      position++;
      continue;  // 6.3
    }

    // Step 7
    StringBuilder token_builder;
    while (position < input_length && IsNotHTMLSpace<UChar>(input[position]))
      token_builder.Append(input[position++]);

    // Step 8
    String token = token_builder.ToString();
    if (tokens.Contains(token)) {
      // Step 8.1
      while (position < input_length && IsHTMLSpace<UChar>(input[position]))
        ++position;

      // Step 8.2
      size_t j = output.length();
      while (j > 0 && IsHTMLSpace<UChar>(output[j - 1]))
        --j;
      output.Resize(j);
    } else {
      output.Append(token);  // Step 9
    }

    if (position < input_length && !output.IsEmpty())
      output.Append(' ');
  }

  size_t j = output.length();
  if (j > 0 && IsHTMLSpace<UChar>(output[j - 1]))
    output.Resize(j - 1);

  return output.ToAtomicString();
}

void DOMTokenList::setValue(const AtomicString& value) {
  bool value_changed = value_ != value;
  value_ = value;
  if (value_changed)
    tokens_.Set(value, SpaceSplitString::kShouldNotFoldCase);
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
