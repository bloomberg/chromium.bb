// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_UTILS_H_
#define XFA_FXFA_PARSER_XFA_UTILS_H_

#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/widestring.h"
#include "xfa/fxfa/fxfa.h"
#include "xfa/fxfa/fxfa_basic.h"
#include "xfa/fxfa/parser/cxfa_localevalue.h"

class CFX_XMLElement;
class CXFA_Node;

bool XFA_FDEExtension_ResolveNamespaceQualifier(CFX_XMLElement* pNode,
                                                const WideString& wsQualifier,
                                                WideString* wsNamespaceURI);

CXFA_LocaleValue XFA_GetLocaleValue(const CXFA_Node* pNode);
CXFA_LocaleValue::ValueType XFA_GetLocaleValueType(XFA_Element element);
int32_t XFA_MapRotation(int32_t nRotation);

bool XFA_RecognizeRichText(CFX_XMLElement* pRichTextXMLNode);
bool XFA_FieldIsMultiListBox(const CXFA_Node* pFieldNode);

void XFA_DataExporter_DealWithDataGroupNode(CXFA_Node* pDataNode);
void XFA_DataExporter_RegenerateFormFile(
    CXFA_Node* pNode,
    const RetainPtr<IFX_SeekableStream>& pStream,
    bool bSaveXML);

void XFA_EventErrorAccumulate(XFA_EventError* pAcc, XFA_EventError eNew);

#endif  // XFA_FXFA_PARSER_XFA_UTILS_H_
