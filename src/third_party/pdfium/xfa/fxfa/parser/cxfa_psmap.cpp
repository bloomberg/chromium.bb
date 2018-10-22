// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_psmap.h"

namespace {

constexpr wchar_t kPsMapName[] = L"psMap";

}  // namespace

CXFA_PsMap::CXFA_PsMap(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::Node,
                XFA_Element::PsMap,
                nullptr,
                nullptr,
                kPsMapName) {}

CXFA_PsMap::~CXFA_PsMap() {}
