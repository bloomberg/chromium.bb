// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableData_h
#define CSSVariableData_h

#include "base/macros.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSParserTokenRange;
class CSSSyntaxDescriptor;
enum class SecureContextMode;

class CORE_EXPORT CSSVariableData : public RefCounted<CSSVariableData> {
  USING_FAST_MALLOC(CSSVariableData);

 public:
  static scoped_refptr<CSSVariableData> Create(const CSSParserTokenRange& range,
                                               bool is_animation_tainted,
                                               bool needs_variable_resolution) {
    return base::AdoptRef(new CSSVariableData(range, is_animation_tainted,
                                              needs_variable_resolution));
  }

  static scoped_refptr<CSSVariableData> CreateResolved(
      const Vector<CSSParserToken>& resolved_tokens,
      Vector<String> backing_strings,
      bool is_animation_tainted) {
    return base::AdoptRef(new CSSVariableData(
        resolved_tokens, std::move(backing_strings), is_animation_tainted));
  }

  CSSParserTokenRange TokenRange() const { return tokens_; }

  const Vector<CSSParserToken>& Tokens() const { return tokens_; }
  const Vector<String>& BackingStrings() const { return backing_strings_; }

  bool operator==(const CSSVariableData& other) const;

  bool IsAnimationTainted() const { return is_animation_tainted_; }

  bool NeedsVariableResolution() const { return needs_variable_resolution_; }

  const CSSValue* ParseForSyntax(const CSSSyntaxDescriptor&,
                                 SecureContextMode) const;

 private:
  CSSVariableData(const CSSParserTokenRange&,
                  bool is_animation_tainted,
                  bool needs_variable_resolution);

  CSSVariableData(const Vector<CSSParserToken>& resolved_tokens,
                  Vector<String> backing_strings,
                  bool is_animation_tainted)
      : backing_strings_(std::move(backing_strings)),
        tokens_(resolved_tokens),
        is_animation_tainted_(is_animation_tainted),
        needs_variable_resolution_(false) {}

  void ConsumeAndUpdateTokens(const CSSParserTokenRange&);

  // tokens_ may have raw pointers to string data, we store the String objects
  // owning that data in backing_strings_ to keep it alive alongside the
  // tokens_.
  Vector<String> backing_strings_;
  Vector<CSSParserToken> tokens_;
  const bool is_animation_tainted_;
  const bool needs_variable_resolution_;
  DISALLOW_COPY_AND_ASSIGN(CSSVariableData);
};

}  // namespace blink

#endif  // CSSVariableData_h
