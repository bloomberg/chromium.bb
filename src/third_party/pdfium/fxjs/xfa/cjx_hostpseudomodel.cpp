// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_hostpseudomodel.h"

#include <memory>
#include <vector>

#include "fxjs/js_resources.h"
#include "fxjs/xfa/cfxjse_engine.h"
#include "fxjs/xfa/cfxjse_value.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/parser/cscript_hostpseudomodel.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"

namespace {

int32_t FilterName(WideStringView wsExpression,
                   int32_t nStart,
                   WideString& wsFilter) {
  ASSERT(nStart > -1);
  int32_t iLength = wsExpression.GetLength();
  if (nStart >= iLength)
    return iLength;

  int32_t nCount = 0;
  {
    // Span's lifetime must end before ReleaseBuffer() below.
    pdfium::span<wchar_t> pBuf = wsFilter.GetBuffer(iLength - nStart);
    const wchar_t* pSrc = wsExpression.unterminated_c_str();
    while (nStart < iLength) {
      wchar_t wCur = pSrc[nStart++];
      if (wCur == ',')
        break;

      pBuf[nCount++] = wCur;
    }
  }
  wsFilter.ReleaseBuffer(nCount);
  wsFilter.Trim();
  return nStart;
}

}  // namespace

const CJX_MethodSpec CJX_HostPseudoModel::MethodSpecs[] = {
    {"beep", beep_static},
    {"documentCountInBatch", documentCountInBatch_static},
    {"documentInBatch", documentInBatch_static},
    {"exportData", exportData_static},
    {"getFocus", getFocus_static},
    {"gotoURL", gotoURL_static},
    {"importData", importData_static},
    {"messageBox", messageBox_static},
    {"openList", openList_static},
    {"pageDown", pageDown_static},
    {"pageUp", pageUp_static},
    {"print", print_static},
    {"resetData", resetData_static},
    {"response", response_static},
    {"setFocus", setFocus_static}};

CJX_HostPseudoModel::CJX_HostPseudoModel(CScript_HostPseudoModel* model)
    : CJX_Object(model) {
  DefineMethods(MethodSpecs);
}

CJX_HostPseudoModel::~CJX_HostPseudoModel() = default;

bool CJX_HostPseudoModel::DynamicTypeIs(TypeTag eType) const {
  return eType == static_type__ || ParentType__::DynamicTypeIs(eType);
}

void CJX_HostPseudoModel::appType(v8::Isolate* pIsolate,
                                  CFXJSE_Value* pValue,
                                  bool bSetting,
                                  XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }
  pValue->SetString(pIsolate, "Exchange");
}

void CJX_HostPseudoModel::calculationsEnabled(v8::Isolate* pIsolate,
                                              CFXJSE_Value* pValue,
                                              bool bSetting,
                                              XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  if (bSetting) {
    hDoc->SetCalculationsEnabled(pValue->ToBoolean(pIsolate));
    return;
  }
  pValue->SetBoolean(pIsolate, hDoc->IsCalculationsEnabled());
}

void CJX_HostPseudoModel::currentPage(v8::Isolate* pIsolate,
                                      CFXJSE_Value* pValue,
                                      bool bSetting,
                                      XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  if (bSetting) {
    hDoc->SetCurrentPage(pValue->ToInteger(pIsolate));
    return;
  }
  pValue->SetInteger(pIsolate, hDoc->GetCurrentPage());
}

void CJX_HostPseudoModel::language(v8::Isolate* pIsolate,
                                   CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  if (bSetting) {
    ThrowException(WideString::FromASCII("Unable to set language value."));
    return;
  }
  pValue->SetString(
      pIsolate,
      pNotify->GetAppProvider()->GetLanguage().ToUTF8().AsStringView());
}

void CJX_HostPseudoModel::numPages(v8::Isolate* pIsolate,
                                   CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  if (bSetting) {
    ThrowException(WideString::FromASCII("Unable to set numPages value."));
    return;
  }
  pValue->SetInteger(pIsolate, hDoc->CountPages());
}

void CJX_HostPseudoModel::platform(v8::Isolate* pIsolate,
                                   CFXJSE_Value* pValue,
                                   bool bSetting,
                                   XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  if (bSetting) {
    ThrowException(WideString::FromASCII("Unable to set platform value."));
    return;
  }
  pValue->SetString(
      pIsolate,
      pNotify->GetAppProvider()->GetPlatform().ToUTF8().AsStringView());
}

void CJX_HostPseudoModel::title(v8::Isolate* pIsolate,
                                CFXJSE_Value* pValue,
                                bool bSetting,
                                XFA_Attribute eAttribute) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return;

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  if (bSetting) {
    hDoc->SetTitle(pValue->ToWideString(pIsolate));
    return;
  }

  WideString wsTitle = hDoc->GetTitle();
  pValue->SetString(pIsolate, wsTitle.ToUTF8().AsStringView());
}

void CJX_HostPseudoModel::validationsEnabled(v8::Isolate* pIsolate,
                                             CFXJSE_Value* pValue,
                                             bool bSetting,
                                             XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  if (bSetting) {
    hDoc->SetValidationsEnabled(pValue->ToBoolean(pIsolate));
    return;
  }

  bool bEnabled = hDoc->IsValidationsEnabled();
  pValue->SetBoolean(pIsolate, bEnabled);
}

void CJX_HostPseudoModel::variation(v8::Isolate* pIsolate,
                                    CFXJSE_Value* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return;

  if (bSetting) {
    ThrowException(WideString::FromASCII("Unable to set variation value."));
    return;
  }
  pValue->SetString(pIsolate, "Full");
}

void CJX_HostPseudoModel::version(v8::Isolate* pIsolate,
                                  CFXJSE_Value* pValue,
                                  bool bSetting,
                                  XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowException(WideString::FromASCII("Unable to set version value."));
    return;
  }
  pValue->SetString(pIsolate, "11");
}

void CJX_HostPseudoModel::name(v8::Isolate* pIsolate,
                               CFXJSE_Value* pValue,
                               bool bSetting,
                               XFA_Attribute eAttribute) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return;

  if (bSetting) {
    ThrowInvalidPropertyException();
    return;
  }
  pValue->SetString(
      pIsolate,
      pNotify->GetAppProvider()->GetAppName().ToUTF8().AsStringView());
}

CJS_Result CJX_HostPseudoModel::gotoURL(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.size() != 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  pNotify->GetFFDoc()->GotoURL(runtime->ToWideString(params[0]));
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::openList(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.size() != 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  CXFA_Node* pNode = nullptr;
  if (params[0]->IsObject()) {
    pNode =
        ToNode(static_cast<CFXJSE_Engine*>(runtime)->ToXFAObject(params[0]));
  } else if (params[0]->IsString()) {
    CFXJSE_Engine* pScriptContext = GetDocument()->GetScriptContext();
    CXFA_Object* pObject = pScriptContext->GetThisObject();
    if (!pObject)
      return CJS_Result::Success();

    uint32_t dwFlag = XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Parent |
                      XFA_RESOLVENODE_Siblings;
    XFA_ResolveNodeRS resolveNodeRS;
    bool bRet = pScriptContext->ResolveObjects(
        pObject, runtime->ToWideString(params[0]).AsStringView(),
        &resolveNodeRS, dwFlag, nullptr);
    if (!bRet || !resolveNodeRS.objects.front()->IsNode())
      return CJS_Result::Success();

    pNode = resolveNodeRS.objects.front()->AsNode();
  }

  if (pNode)
    pNotify->OpenDropDownList(pNode);

  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::response(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (params.empty() || params.size() > 4)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  WideString question;
  if (params.size() >= 1)
    question = runtime->ToWideString(params[0]);

  WideString title;
  if (params.size() >= 2)
    title = runtime->ToWideString(params[1]);

  WideString defaultAnswer;
  if (params.size() >= 3)
    defaultAnswer = runtime->ToWideString(params[2]);

  bool mark = false;
  if (params.size() >= 4)
    mark = runtime->ToInt32(params[3]) != 0;

  WideString answer =
      pNotify->GetAppProvider()->Response(question, title, defaultAnswer, mark);
  return CJS_Result::Success(
      runtime->NewString(answer.ToUTF8().AsStringView()));
}

CJS_Result CJX_HostPseudoModel::documentInBatch(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  return CJS_Result::Success(runtime->NewNumber(0));
}

CJS_Result CJX_HostPseudoModel::resetData(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (params.size() > 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  WideString expression;
  if (params.size() >= 1)
    expression = runtime->ToWideString(params[0]);

  if (expression.IsEmpty()) {
    pNotify->ResetData(nullptr);
    return CJS_Result::Success();
  }

  int32_t iStart = 0;
  WideString wsName;
  CXFA_Node* pNode = nullptr;
  int32_t iExpLength = expression.GetLength();
  while (iStart < iExpLength) {
    iStart = FilterName(expression.AsStringView(), iStart, wsName);
    CFXJSE_Engine* pScriptContext = GetDocument()->GetScriptContext();
    CXFA_Object* pObject = pScriptContext->GetThisObject();
    if (!pObject)
      return CJS_Result::Success();

    uint32_t dwFlag = XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Parent |
                      XFA_RESOLVENODE_Siblings;
    XFA_ResolveNodeRS resolveNodeRS;
    bool bRet = pScriptContext->ResolveObjects(pObject, wsName.AsStringView(),
                                               &resolveNodeRS, dwFlag, nullptr);
    if (!bRet || !resolveNodeRS.objects.front()->IsNode())
      continue;

    pNode = resolveNodeRS.objects.front()->AsNode();
    pNotify->ResetData(pNode->IsWidgetReady() ? pNode : nullptr);
  }
  if (!pNode)
    pNotify->ResetData(nullptr);

  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::beep(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.size() > 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  uint32_t dwType = 4;
  if (params.size() >= 1)
    dwType = runtime->ToInt32(params[0]);

  pNotify->GetAppProvider()->Beep(dwType);
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::setFocus(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.size() != 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  CXFA_Node* pNode = nullptr;
  if (params.size() >= 1) {
    if (params[0]->IsObject()) {
      pNode =
          ToNode(static_cast<CFXJSE_Engine*>(runtime)->ToXFAObject(params[0]));
    } else if (params[0]->IsString()) {
      CFXJSE_Engine* pScriptContext = GetDocument()->GetScriptContext();
      CXFA_Object* pObject = pScriptContext->GetThisObject();
      if (!pObject)
        return CJS_Result::Success();

      uint32_t dwFlag = XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Parent |
                        XFA_RESOLVENODE_Siblings;
      XFA_ResolveNodeRS resolveNodeRS;
      bool bRet = pScriptContext->ResolveObjects(
          pObject, runtime->ToWideString(params[0]).AsStringView(),
          &resolveNodeRS, dwFlag, nullptr);
      if (!bRet || !resolveNodeRS.objects.front()->IsNode())
        return CJS_Result::Success();

      pNode = resolveNodeRS.objects.front()->AsNode();
    }
  }
  pNotify->SetFocusWidgetNode(pNode);
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::getFocus(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  CXFA_Node* pNode = pNotify->GetFocusWidgetNode();
  if (!pNode)
    return CJS_Result::Success();

  v8::Local<v8::Value> value =
      GetDocument()->GetScriptContext()->GetOrCreateJSBindingFromMap(pNode);

  return CJS_Result::Success(value);
}

CJS_Result CJX_HostPseudoModel::messageBox(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.empty() || params.size() > 4)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  WideString message;
  if (params.size() >= 1)
    message = runtime->ToWideString(params[0]);

  WideString title;
  if (params.size() >= 2)
    title = runtime->ToWideString(params[1]);

  uint32_t messageType = static_cast<uint32_t>(AlertIcon::kDefault);
  if (params.size() >= 3) {
    messageType = runtime->ToInt32(params[2]);
    if (messageType > static_cast<uint32_t>(AlertIcon::kStatus))
      messageType = static_cast<uint32_t>(AlertIcon::kDefault);
  }

  uint32_t buttonType = static_cast<uint32_t>(AlertButton::kDefault);
  if (params.size() >= 4) {
    buttonType = runtime->ToInt32(params[3]);
    if (buttonType > static_cast<uint32_t>(AlertButton::kYesNoCancel))
      buttonType = static_cast<uint32_t>(AlertButton::kDefault);
  }

  int32_t iValue = pNotify->GetAppProvider()->MsgBox(message, title,
                                                     messageType, buttonType);
  return CJS_Result::Success(runtime->NewNumber(iValue));
}

CJS_Result CJX_HostPseudoModel::documentCountInBatch(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  return CJS_Result::Success(runtime->NewNumber(0));
}

CJS_Result CJX_HostPseudoModel::print(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (!GetDocument()->GetScriptContext()->IsRunAtClient())
    return CJS_Result::Success();

  if (params.size() != 8)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  uint32_t dwOptions = 0;
  if (runtime->ToBoolean(params[0]))
    dwOptions |= XFA_PRINTOPT_ShowDialog;
  if (runtime->ToBoolean(params[3]))
    dwOptions |= XFA_PRINTOPT_CanCancel;
  if (runtime->ToBoolean(params[4]))
    dwOptions |= XFA_PRINTOPT_ShrinkPage;
  if (runtime->ToBoolean(params[5]))
    dwOptions |= XFA_PRINTOPT_AsImage;
  if (runtime->ToBoolean(params[6]))
    dwOptions |= XFA_PRINTOPT_ReverseOrder;
  if (runtime->ToBoolean(params[7]))
    dwOptions |= XFA_PRINTOPT_PrintAnnot;

  int32_t nStartPage = runtime->ToInt32(params[1]);
  int32_t nEndPage = runtime->ToInt32(params[2]);
  pNotify->GetFFDoc()->Print(nStartPage, nEndPage, dwOptions);
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::importData(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (params.empty() || params.size() > 1)
    return CJS_Result::Failure(JSMessage::kParamError);

  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::exportData(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  if (params.empty() || params.size() > 2)
    return CJS_Result::Failure(JSMessage::kParamError);

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  WideString filePath;
  if (params.size() >= 1)
    filePath = runtime->ToWideString(params[0]);

  bool XDP = true;
  if (params.size() >= 2)
    XDP = runtime->ToBoolean(params[1]);

  pNotify->GetFFDoc()->ExportData(filePath, XDP);
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::pageUp(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  int32_t nCurPage = hDoc->GetCurrentPage();
  if (nCurPage <= 1)
    return CJS_Result::Success();

  hDoc->SetCurrentPage(nCurPage - 1);
  return CJS_Result::Success();
}

CJS_Result CJX_HostPseudoModel::pageDown(
    CFX_V8* runtime,
    const std::vector<v8::Local<v8::Value>>& params) {
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify)
    return CJS_Result::Success();

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  int32_t nCurPage = hDoc->GetCurrentPage();
  int32_t nPageCount = hDoc->CountPages();
  if (!nPageCount || nCurPage == nPageCount)
    return CJS_Result::Success();

  int32_t nNewPage = 0;
  if (nCurPage >= nPageCount)
    nNewPage = nPageCount - 1;
  else
    nNewPage = nCurPage + 1;

  hDoc->SetCurrentPage(nNewPage);
  return CJS_Result::Success();
}
