// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_signdata.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kSignDataPropertyData[] = {
    {XFA_Element::Filter, 1, {}},
    {XFA_Element::Manifest, 1, {}},
};

const CXFA_Node::AttributeData kSignDataAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Ref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Operation, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Sign},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Target, XFA_AttributeType::CData, nullptr},
};

}  // namespace

CXFA_SignData::CXFA_SignData(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::Node,
                XFA_Element::SignData,
                kSignDataPropertyData,
                kSignDataAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_SignData::~CXFA_SignData() = default;
