// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_items.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::AttributeData kItemsAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Ref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Presence, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Visible},
    {XFA_Attribute::Save, XFA_AttributeType::Boolean, (void*)0},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
};

}  // namespace

CXFA_Items::CXFA_Items(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::Node,
                XFA_Element::Items,
                {},
                kItemsAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_Items::~CXFA_Items() = default;
