// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_operation.h"

#include "fxjs/xfa/cjx_textnode.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::AttributeData kOperationAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Name, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Output, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Input, XFA_AttributeType::CData, nullptr},
};

}  // namespace

CXFA_Operation::CXFA_Operation(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_ConnectionSet,
                XFA_ObjectType::TextNode,
                XFA_Element::Operation,
                {},
                kOperationAttributeData,
                pdfium::MakeUnique<CJX_TextNode>(this)) {}

CXFA_Operation::~CXFA_Operation() = default;
