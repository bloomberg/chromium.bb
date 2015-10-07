// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/CSSCustomIdentValue.h"

#include "core/css/CSSMarkup.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const String& str)
    : CSSValue(CustomIdentClass)
    , m_string(str) { }

String CSSCustomIdentValue::customCSSText() const
{
    return quoteCSSStringIfNeeded(m_string);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSCustomIdentValue)
{
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink
