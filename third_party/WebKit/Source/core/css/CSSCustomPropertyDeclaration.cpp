// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/CSSCustomPropertyDeclaration.h"

#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

DEFINE_TRACE_AFTER_DISPATCH(CSSCustomPropertyDeclaration)
{
    CSSValue::traceAfterDispatch(visitor);
}

String CSSCustomPropertyDeclaration::customCSSText() const
{
    return m_value->tokenRange().serialize();
}

} // namespace blink
