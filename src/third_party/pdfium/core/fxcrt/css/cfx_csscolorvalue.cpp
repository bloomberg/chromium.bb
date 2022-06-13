// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/css/cfx_csscolorvalue.h"

CFX_CSSColorValue::CFX_CSSColorValue(FX_ARGB value)
    : CFX_CSSValue(PrimitiveType::kRGB), value_(value) {}

CFX_CSSColorValue::~CFX_CSSColorValue() = default;
