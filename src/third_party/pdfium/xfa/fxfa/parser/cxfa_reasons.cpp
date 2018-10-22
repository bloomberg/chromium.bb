// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_reasons.h"

#include "fxjs/xfa/cjx_reasons.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::AttributeData kReasonsAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Type, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Optional},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kReasonsName[] = L"reasons";

}  // namespace

CXFA_Reasons::CXFA_Reasons(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                (XFA_XDPPACKET_Template | XFA_XDPPACKET_Form),
                XFA_ObjectType::Node,
                XFA_Element::Reasons,
                nullptr,
                kReasonsAttributeData,
                kReasonsName,
                pdfium::MakeUnique<CJX_Reasons>(this)) {}

CXFA_Reasons::~CXFA_Reasons() {}
