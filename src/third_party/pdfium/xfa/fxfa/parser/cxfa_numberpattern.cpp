// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_numberpattern.h"

namespace {

const CXFA_Node::AttributeData kNumberPatternAttributeData[] = {
    {XFA_Attribute::Name, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Numeric},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kNumberPatternName[] = L"numberPattern";

}  // namespace

CXFA_NumberPattern::CXFA_NumberPattern(CXFA_Document* doc,
                                       XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::ContentNode,
                XFA_Element::NumberPattern,
                nullptr,
                kNumberPatternAttributeData,
                kNumberPatternName) {}

CXFA_NumberPattern::~CXFA_NumberPattern() {}
