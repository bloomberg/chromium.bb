// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_manifest.h"

#include "fxjs/xfa/cjx_manifest.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kManifestPropertyData[] = {
    {XFA_Element::Extras, 1, {}},
};

const CXFA_Node::AttributeData kManifestAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Action, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Include},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
};

}  // namespace

CXFA_Manifest::CXFA_Manifest(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                {XFA_XDPPACKET::kTemplate, XFA_XDPPACKET::kForm},
                XFA_ObjectType::Node,
                XFA_Element::Manifest,
                kManifestPropertyData,
                kManifestAttributeData,
                cppgc::MakeGarbageCollected<CJX_Manifest>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_Manifest::~CXFA_Manifest() = default;
