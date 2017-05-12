// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableParser_h
#define CSSVariableParser_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CSSCustomPropertyDeclaration;
class CSSParserContext;
class CSSVariableReferenceValue;

class CORE_EXPORT CSSVariableParser {
 public:
  static bool ContainsValidVariableReferences(CSSParserTokenRange);

  static CSSCustomPropertyDeclaration* ParseDeclarationValue(
      const AtomicString&,
      CSSParserTokenRange,
      bool is_animation_tainted);
  static CSSVariableReferenceValue* ParseRegisteredPropertyValue(
      CSSParserTokenRange,
      const CSSParserContext&,
      bool require_var_reference,
      bool is_animation_tainted);

  static bool IsValidVariableName(const CSSParserToken&);
  static bool IsValidVariableName(const String&);
};

}  // namespace blink

#endif  // CSSVariableParser_h
