// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_era.h"

namespace {

constexpr wchar_t kEraName[] = L"era";

}  // namespace

CXFA_Era::CXFA_Era(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::ContentNode,
                XFA_Element::Era,
                nullptr,
                nullptr,
                kEraName) {}

CXFA_Era::~CXFA_Era() {}
