// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_pagearea.h"

#include "fxjs/xfa/cjx_container.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kPageAreaPropertyData[] = {
    {XFA_Element::Medium, 1, {}},
    {XFA_Element::Desc, 1, {}},
    {XFA_Element::Extras, 1, {}},
    {XFA_Element::Occur, 1, {}},
};

const CXFA_Node::AttributeData kPageAreaAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::PagePosition, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Any},
    {XFA_Attribute::OddOrEven, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Any},
    {XFA_Attribute::Relevant, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::InitialNumber, XFA_AttributeType::Integer, (void*)1},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Numbered, XFA_AttributeType::Integer, (void*)1},
    {XFA_Attribute::BlankOrNotBlank, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Any},
};

}  // namespace

CXFA_PageArea::CXFA_PageArea(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::ContainerNode,
                XFA_Element::PageArea,
                kPageAreaPropertyData,
                kPageAreaAttributeData,
                cppgc::MakeGarbageCollected<CJX_Container>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_PageArea::~CXFA_PageArea() = default;
