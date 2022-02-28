// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_hyphenation.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::AttributeData kHyphenationAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::WordCharacterCount, XFA_AttributeType::Integer, (void*)7},
    {XFA_Attribute::Hyphenate, XFA_AttributeType::Boolean, (void*)0},
    {XFA_Attribute::ExcludeInitialCap, XFA_AttributeType::Boolean, (void*)0},
    {XFA_Attribute::PushCharacterCount, XFA_AttributeType::Integer, (void*)3},
    {XFA_Attribute::RemainCharacterCount, XFA_AttributeType::Integer, (void*)3},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::ExcludeAllCaps, XFA_AttributeType::Boolean, (void*)0},
};

}  // namespace

CXFA_Hyphenation::CXFA_Hyphenation(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::Node,
                XFA_Element::Hyphenation,
                {},
                kHyphenationAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_Hyphenation::~CXFA_Hyphenation() = default;
