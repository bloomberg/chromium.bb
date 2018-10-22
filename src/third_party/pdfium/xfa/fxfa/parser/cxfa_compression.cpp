// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_compression.h"

namespace {

const CXFA_Node::PropertyData kCompressionPropertyData[] = {
    {XFA_Element::Level, 1, 0},
    {XFA_Element::Type, 1, 0},
    {XFA_Element::CompressObjectStream, 1, 0},
    {XFA_Element::CompressLogicalStructure, 1, 0},
    {XFA_Element::Unknown, 0, 0}};
const CXFA_Node::AttributeData kCompressionAttributeData[] = {
    {XFA_Attribute::Desc, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kCompressionName[] = L"compression";

}  // namespace

CXFA_Compression::CXFA_Compression(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::Node,
                XFA_Element::Compression,
                kCompressionPropertyData,
                kCompressionAttributeData,
                kCompressionName) {}

CXFA_Compression::~CXFA_Compression() {}
