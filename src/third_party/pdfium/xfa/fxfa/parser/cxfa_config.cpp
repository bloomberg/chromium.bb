// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_config.h"

namespace {

const CXFA_Node::PropertyData kConfigPropertyData[] = {
    {XFA_Element::Present, 1, 0},
    {XFA_Element::Acrobat, 1, 0},
    {XFA_Element::Trace, 1, 0},
    {XFA_Element::Unknown, 0, 0}};
const CXFA_Node::AttributeData kConfigAttributeData[] = {
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kConfigName[] = L"config";

}  // namespace

CXFA_Config::CXFA_Config(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::ModelNode,
                XFA_Element::Config,
                kConfigPropertyData,
                kConfigAttributeData,
                kConfigName) {}

CXFA_Config::~CXFA_Config() {}
