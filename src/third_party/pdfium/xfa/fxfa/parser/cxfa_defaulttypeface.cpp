// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_defaulttypeface.h"

#include "fxjs/xfa/cjx_node.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::AttributeData kDefaultTypefaceAttributeData[] = {
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::WritingScript, XFA_AttributeType::Enum,
     (void*)XFA_AttributeValue::Asterisk},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
};

}  // namespace

CXFA_DefaultTypeface::CXFA_DefaultTypeface(CXFA_Document* doc,
                                           XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::NodeV,
                XFA_Element::DefaultTypeface,
                {},
                kDefaultTypefaceAttributeData,
                pdfium::MakeUnique<CJX_Node>(this)) {}

CXFA_DefaultTypeface::~CXFA_DefaultTypeface() = default;
