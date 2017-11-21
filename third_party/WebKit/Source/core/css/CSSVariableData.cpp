// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSVariableData.h"

#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/parser/CSSParserContext.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

template <typename CharacterType>
static void UpdateTokens(const CSSParserTokenRange& range,
                         const String& backing_string,
                         Vector<CSSParserToken>& result) {
  const CharacterType* current_offset =
      backing_string.GetCharacters<CharacterType>();
  for (const CSSParserToken& token : range) {
    if (token.HasStringBacking()) {
      unsigned length = token.Value().length();
      StringView string(current_offset, length);
      result.push_back(token.CopyWithUpdatedString(string));
      current_offset += length;
    } else {
      result.push_back(token);
    }
  }
  DCHECK(current_offset == backing_string.GetCharacters<CharacterType>() +
                               backing_string.length());
}

bool CSSVariableData::operator==(const CSSVariableData& other) const {
  return Tokens() == other.Tokens();
}

void CSSVariableData::ConsumeAndUpdateTokens(const CSSParserTokenRange& range) {
  DCHECK_EQ(tokens_.size(), 0u);
  DCHECK_EQ(backing_strings_.size(), 0u);
  StringBuilder string_builder;
  CSSParserTokenRange local_range = range;

  while (!local_range.AtEnd()) {
    CSSParserToken token = local_range.Consume();
    if (token.HasStringBacking())
      string_builder.Append(token.Value());
  }
  String backing_string = string_builder.ToString();
  backing_strings_.push_back(backing_string);
  if (backing_string.Is8Bit())
    UpdateTokens<LChar>(range, backing_string, tokens_);
  else
    UpdateTokens<UChar>(range, backing_string, tokens_);
}

CSSVariableData::CSSVariableData(const CSSParserTokenRange& range,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution)
    : is_animation_tainted_(is_animation_tainted),
      needs_variable_resolution_(needs_variable_resolution) {
  DCHECK(!range.AtEnd());
  ConsumeAndUpdateTokens(range);
}

const CSSValue* CSSVariableData::ParseForSyntax(
    const CSSSyntaxDescriptor& syntax,
    SecureContextMode secure_context_mode) const {
  DCHECK(!NeedsVariableResolution());
  // TODO(timloh): This probably needs a proper parser context for
  // relative URL resolution.
  return syntax.Parse(TokenRange(), StrictCSSParserContext(secure_context_mode),
                      is_animation_tainted_);
}

}  // namespace blink
