// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_ps.h"

#include "fxjs/xfa/cjx_node.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::PropertyData kPsPropertyData[] = {
    {XFA_Element::FontInfo, 1, 0},  {XFA_Element::Jog, 1, 0},
    {XFA_Element::Xdc, 1, 0},       {XFA_Element::BatchOutput, 1, 0},
    {XFA_Element::OutputBin, 1, 0}, {XFA_Element::Compress, 1, 0},
    {XFA_Element::Staple, 1, 0},    {XFA_Element::MediumInfo, 1, 0},
};

const CXFA_Node::AttributeData kPsAttributeData[] = {
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
};

}  // namespace

CXFA_Ps::CXFA_Ps(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::Node,
                XFA_Element::Ps,
                kPsPropertyData,
                kPsAttributeData,
                pdfium::MakeUnique<CJX_Node>(this)) {}

CXFA_Ps::~CXFA_Ps() = default;
