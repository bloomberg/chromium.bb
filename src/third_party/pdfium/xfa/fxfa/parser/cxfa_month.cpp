// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_month.h"

namespace {

constexpr wchar_t kMonthName[] = L"month";

}  // namespace

CXFA_Month::CXFA_Month(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::ContentNode,
                XFA_Element::Month,
                nullptr,
                nullptr,
                kMonthName) {}

CXFA_Month::~CXFA_Month() {}
