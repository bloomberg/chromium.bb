// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_subjectdns.h"

#include "fxjs/xfa/cjx_subjectdns.h"
#include "third_party/base/ptr_util.h"

namespace {

const CXFA_Node::AttributeData kSubjectDNsAttributeData[] = {
    {XFA_Attribute::Id, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Use, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Type, XFA_AttributeType::Enum,
     (void*)XFA_AttributeEnum::Optional},
    {XFA_Attribute::Usehref, XFA_AttributeType::CData, nullptr},
    {XFA_Attribute::Unknown, XFA_AttributeType::Integer, nullptr}};

constexpr wchar_t kSubjectDNsName[] = L"subjectDNs";

}  // namespace

CXFA_SubjectDNs::CXFA_SubjectDNs(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                (XFA_XDPPACKET_Template | XFA_XDPPACKET_Form),
                XFA_ObjectType::Node,
                XFA_Element::SubjectDNs,
                nullptr,
                kSubjectDNsAttributeData,
                kSubjectDNsName,
                pdfium::MakeUnique<CJX_SubjectDNs>(this)) {}

CXFA_SubjectDNs::~CXFA_SubjectDNs() {}
