// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_permissions.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kPermissionsPropertyData[] = {
    {XFA_Element::ModifyAnnots, 1, {}},
    {XFA_Element::ContentCopy, 1, {}},
    {XFA_Element::FormFieldFilling, 1, {}},
    {XFA_Element::Change, 1, {}},
    {XFA_Element::AccessibleContent, 1, {}},
    {XFA_Element::Print, 1, {}},
    {XFA_Element::PlaintextMetadata, 1, {}},
    {XFA_Element::PrintHighQuality, 1, {}},
    {XFA_Element::DocumentAssembly, 1, {}},
};

const CXFA_Node::AttributeData kPermissionsAttributeData[] = {
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
};

}  // namespace

CXFA_Permissions::CXFA_Permissions(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET::kConfig,
                XFA_ObjectType::Node,
                XFA_Element::Permissions,
                kPermissionsPropertyData,
                kPermissionsAttributeData,
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_Permissions::~CXFA_Permissions() = default;
