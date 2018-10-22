// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_versioncontrol.h"

namespace {

const CXFA_Node::AttributeData kVersionControlAttributeData[] = {
    {XFA_Attribute::SourceBelow, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Update},
    {XFA_Attribute::OutputBelow, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Warn},
    {XFA_Attribute::SourceAbove, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Warn},
    {XFA_Attribute::Lock, XFA_AttributeType::Integer, (void*)0},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kVersionControlName[] = L"versionControl";

}  // namespace

CXFA_VersionControl::CXFA_VersionControl(CXFA_Document* doc,
                                         XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Config,
                XFA_ObjectType::Node,
                XFA_Element::VersionControl,
                nullptr,
                kVersionControlAttributeData,
                kVersionControlName) {}

CXFA_VersionControl::~CXFA_VersionControl() {}
