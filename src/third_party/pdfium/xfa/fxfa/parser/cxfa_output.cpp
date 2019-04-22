// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_output.h"

#include "fxjs/xfa/cjx_node.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::PropertyData kOutputPropertyData[] = {
    {XFA_Element::To, 1, 0},
    {XFA_Element::Uri, 1, 0},
    {XFA_Element::Type, 1, 0},
};

const CXFA_Node::AttributeData kOutputAttributeData[] = {
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
};

}  // namespace

CXFA_Output::CXFA_Output(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::Node,
                XFA_Element::Output,
                kOutputPropertyData,
                kOutputAttributeData,
                pdfium::MakeUnique<CJX_Node>(this)) {}

CXFA_Output::~CXFA_Output() = default;
