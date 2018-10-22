// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_datamodel.h"

#include "fxjs/xfa/cjx_model.h"
#include "third_party/base/ptr_util.h"

namespace {

constexpr wchar_t kDataModelName[] = L"dataModel";

}  // namespace

CXFA_DataModel::CXFA_DataModel(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Datasets,
                XFA_ObjectType::ModelNode,
                XFA_Element::DataModel,
                nullptr,
                nullptr,
                kDataModelName,
                pdfium::MakeUnique<CJX_Model>(this)) {}

CXFA_DataModel::~CXFA_DataModel() {}
