// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_versioncontrol.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::AttributeData kVersionControlAttributeData[] = {
    {XFA_Attribute::SourceBelow, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Update},
    {XFA_Attribute::OutputBelow, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Warn},
    {XFA_Attribute::SourceAbove, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Warn},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
};

}  // namespace

CXFA_VersionControl::CXFA_VersionControl(CXFA_Document* doc,
                                         XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET::kConfig,
                XFA_ObjectType::Node,
                XFA_Element::VersionControl,
                {},
                kVersionControlAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_VersionControl::~CXFA_VersionControl() = default;
