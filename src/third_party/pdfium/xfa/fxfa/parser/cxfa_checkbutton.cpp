// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_checkbutton.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kCheckButtonPropertyData[] = {
    {XFA_Element::Margin, 1, {}},
    {XFA_Element::Border, 1, {}},
    {XFA_Element::Extras, 1, {}},
};

const CXFA_Node::AttributeData kCheckButtonAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::AllowNeutral, XFA_AttributeType::Boolean, (void*)0},
    {XFA_Attribute::Mark, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Default},
    {XFA_Attribute::Shape, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Square},
    {XFA_Attribute::Size, XFA_AttributeType::Measure, (void*)L"10pt"},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
};

}  // namespace

// static
CXFA_CheckButton* CXFA_CheckButton::FromNode(CXFA_Node* pNode) {
  return pNode && pNode->GetElementType() == XFA_Element::CheckButton
             ? static_cast<CXFA_CheckButton*>(pNode)
             : nullptr;
}

CXFA_CheckButton::CXFA_CheckButton(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::Node,
                XFA_Element::CheckButton,
                kCheckButtonPropertyData,
                kCheckButtonAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_CheckButton::~CXFA_CheckButton() = default;

XFA_FFWidgetType CXFA_CheckButton::GetDefaultFFWidgetType() const {
  return XFA_FFWidgetType::kCheckButton;
}

bool CXFA_CheckButton::IsRound() {
  return JSObject()->GetEnum(XFA_Attribute::Shape) == XFA_AttributeValue::Round;
}

XFA_AttributeValue CXFA_CheckButton::GetMark() {
  return JSObject()->GetEnum(XFA_Attribute::Mark);
}

bool CXFA_CheckButton::IsAllowNeutral() {
  return JSObject()->GetBoolean(XFA_Attribute::AllowNeutral);
}
