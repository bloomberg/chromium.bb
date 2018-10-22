// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_eranames.h"

namespace {

const CXFA_Node::PropertyData kEraNamesPropertyData[] = {
    {XFA_Element::Era, 2, 0},
    {XFA_Element::Unknown, 0, 0}};

constexpr wchar_t kEraNamesName[] = L"eraNames";

}  // namespace

CXFA_EraNames::CXFA_EraNames(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::Node,
                XFA_Element::EraNames,
                kEraNamesPropertyData,
                nullptr,
                kEraNamesName) {}

CXFA_EraNames::~CXFA_EraNames() {}
