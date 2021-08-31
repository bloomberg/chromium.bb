// Copyright 2021 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_themetext.h"

CFWL_ThemeText::CFWL_ThemeText(CFWL_Widget* pWidget,
                               CFGAS_GEGraphics* pGraphics)
    : CFWL_ThemePart(pWidget), m_pGraphics(pGraphics) {}

CFWL_ThemeText::~CFWL_ThemeText() = default;
