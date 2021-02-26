// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cfxjse_engine.h"

#include <utility>

#include "core/fxcrt/autorestorer.h"
#include "core/fxcrt/cfx_widetextbuf.h"
#include "core/fxcrt/fx_extension.h"
#include "fxjs/cjs_runtime.h"
#include "fxjs/fxv8.h"
#include "fxjs/xfa/cfxjse_class.h"
#include "fxjs/xfa/cfxjse_context.h"
#include "fxjs/xfa/cfxjse_formcalc_context.h"
#include "fxjs/xfa/cfxjse_isolatetracker.h"
#include "fxjs/xfa/cfxjse_nodehelper.h"
#include "fxjs/xfa/cfxjse_resolveprocessor.h"
#include "fxjs/xfa/cfxjse_value.h"
#include "fxjs/xfa/cjx_object.h"
#include "third_party/base/stl_util.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_localemgr.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_object.h"
#include "xfa/fxfa/parser/cxfa_thisproxy.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"
#include "xfa/fxfa/parser/xfa_utils.h"

using pdfium::fxjse::kClassTag;

const FXJSE_CLASS_DESCRIPTOR GlobalClassDescriptor = {
    kClassTag,  // tag
    "Root",     // name
    nullptr,    // methods
    0,          // method count
    CFXJSE_Engine::GlobalPropTypeGetter,
    CFXJSE_Engine::GlobalPropertyGetter,
    CFXJSE_Engine::GlobalPropertySetter,
    CFXJSE_Engine::NormalMethodCall,
};

const FXJSE_CLASS_DESCRIPTOR NormalClassDescriptor = {
    kClassTag,    // tag
    "XFAObject",  // name
    nullptr,      // methods
    0,            // method count
    CFXJSE_Engine::NormalPropTypeGetter,
    CFXJSE_Engine::NormalPropertyGetter,
    CFXJSE_Engine::NormalPropertySetter,
    CFXJSE_Engine::NormalMethodCall,
};

const FXJSE_CLASS_DESCRIPTOR VariablesClassDescriptor = {
    kClassTag,          // tag
    "XFAScriptObject",  // name
    nullptr,            // methods
    0,                  // method count
    CFXJSE_Engine::NormalPropTypeGetter,
    CFXJSE_Engine::GlobalPropertyGetter,
    CFXJSE_Engine::GlobalPropertySetter,
    CFXJSE_Engine::NormalMethodCall,
};

namespace {

const char kFormCalcRuntime[] = "pfm_rt";

}  // namespace

// static
CXFA_Object* CFXJSE_Engine::ToObject(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToObject(info.GetIsolate(), info.Holder());
}

// static
CXFA_Object* CFXJSE_Engine::ToObject(v8::Isolate* pIsolate,
                                     v8::Local<v8::Value> value) {
  if (!value->IsObject())
    return nullptr;

  return ToObject(FXJSE_RetrieveObjectBinding(value.As<v8::Object>()));
}

// static.
CXFA_Object* CFXJSE_Engine::ToObject(v8::Isolate* pIsolate,
                                     CFXJSE_Value* pValue) {
  return ToObject(pValue->ToHostObject(pIsolate));
}

// static
CXFA_Object* CFXJSE_Engine::ToObject(CFXJSE_HostObject* pHostObj) {
  if (!pHostObj)
    return nullptr;

  CJX_Object* pJSObject = pHostObj->AsCJXObject();
  return pJSObject ? pJSObject->GetXFAObject() : nullptr;
}

CFXJSE_Engine::CFXJSE_Engine(CXFA_Document* pDocument,
                             CJS_Runtime* fxjs_runtime)
    : CFX_V8(fxjs_runtime->GetIsolate()),
      m_pSubordinateRuntime(fxjs_runtime),
      m_pDocument(pDocument),
      m_JsContext(CFXJSE_Context::Create(fxjs_runtime->GetIsolate(),
                                         &GlobalClassDescriptor,
                                         pDocument->GetRoot()->JSObject(),
                                         nullptr)),
      m_ResolveProcessor(std::make_unique<CFXJSE_ResolveProcessor>()) {
  RemoveBuiltInObjs(m_JsContext.get());
  m_JsContext->EnableCompatibleMode();

  // Don't know if this can happen before we remove the builtin objs and set
  // compatibility mode.
  m_pJsClass =
      CFXJSE_Class::Create(m_JsContext.get(), &NormalClassDescriptor, false);
}

CFXJSE_Engine::~CFXJSE_Engine() {
  // This is what ensures that the v8 object bound to a CJX_Object
  // no longer retains that binding since it will outlive that object.
  CFXJSE_ScopeUtil_IsolateHandleContext scope(this);
  for (const auto& pair : m_mapObjectToObject) {
    const v8::Global<v8::Object>& binding = pair.second;
    FXJSE_ClearObjectBinding(v8::Local<v8::Object>::New(GetIsolate(), binding));
  }
}

bool CFXJSE_Engine::RunScript(CXFA_Script::Type eScriptType,
                              WideStringView wsScript,
                              CFXJSE_Value* hRetValue,
                              CXFA_Object* pThisObject) {
  CFXJSE_ScopeUtil_IsolateHandleContext scope(this);
  AutoRestorer<CXFA_Script::Type> typeRestorer(&m_eScriptType);
  m_eScriptType = eScriptType;

  ByteString btScript;
  if (eScriptType == CXFA_Script::Type::Formcalc) {
    if (!m_FM2JSContext) {
      m_FM2JSContext = std::make_unique<CFXJSE_FormCalcContext>(
          GetIsolate(), m_JsContext.get(), m_pDocument.Get());
    }
    Optional<CFX_WideTextBuf> wsJavaScript =
        CFXJSE_FormCalcContext::Translate(m_pDocument->GetHeap(), wsScript);
    if (!wsJavaScript.has_value()) {
      hRetValue->SetUndefined(GetIsolate());
      return false;
    }
    btScript = FX_UTF8Encode(wsJavaScript.value().AsStringView());
  } else {
    btScript = FX_UTF8Encode(wsScript);
  }
  AutoRestorer<cppgc::Persistent<CXFA_Object>> nodeRestorer(&m_pThisObject);
  m_pThisObject = pThisObject;

  v8::Local<v8::Object> pThisBinding;
  if (pThisObject)
    pThisBinding = GetOrCreateJSBindingFromMap(pThisObject);

  IJS_Runtime::ScopedEventContext ctx(m_pSubordinateRuntime.Get());
  return m_JsContext->ExecuteScript(btScript.c_str(), hRetValue, pThisBinding);
}

bool CFXJSE_Engine::QueryNodeByFlag(CXFA_Node* refNode,
                                    WideStringView propname,
                                    CFXJSE_Value* pValue,
                                    uint32_t dwFlag,
                                    bool bSetting) {
  if (!refNode)
    return false;

  XFA_ResolveNodeRS resolveRs;
  if (!ResolveObjects(refNode, propname, &resolveRs, dwFlag, nullptr))
    return false;
  if (resolveRs.dwFlags == XFA_ResolveNodeRS::Type::kNodes) {
    pValue->ForceSetValue(GetIsolate(), GetOrCreateJSBindingFromMap(
                                            resolveRs.objects.front().Get()));
    return true;
  }
  if (resolveRs.dwFlags == XFA_ResolveNodeRS::Type::kAttribute &&
      resolveRs.script_attribute.callback) {
    CJX_Object* jsObject = resolveRs.objects.front()->JSObject();
    (*resolveRs.script_attribute.callback)(
        GetIsolate(), jsObject, pValue, bSetting,
        resolveRs.script_attribute.attribute);
  }
  return true;
}

// static
void CFXJSE_Engine::GlobalPropertySetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pObject,
                                         ByteStringView szPropName,
                                         CFXJSE_Value* pValue) {
  CXFA_Object* lpOrginalNode = ToObject(pIsolate, pObject);
  CXFA_Document* pDoc = lpOrginalNode->GetDocument();
  CFXJSE_Engine* lpScriptContext = pDoc->GetScriptContext();
  CXFA_Node* pRefNode = ToNode(lpScriptContext->GetThisObject());
  if (lpOrginalNode->IsThisProxy())
    pRefNode = ToNode(lpScriptContext->GetVariablesThis(lpOrginalNode));

  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (lpScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), pValue,
          XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings |
              XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Properties |
              XFA_RESOLVENODE_Attributes,
          true)) {
    return;
  }
  if (lpOrginalNode->IsThisProxy() && pValue && pValue->IsUndefined(pIsolate)) {
    fxv8::ReentrantDeleteObjectPropertyHelper(lpScriptContext->GetIsolate(),
                                              pObject, szPropName);
    return;
  }
  CXFA_FFNotify* pNotify = pDoc->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  auto* pCJSRuntime = static_cast<CJS_Runtime*>(hDoc->GetIJSRuntime());
  if (!pCJSRuntime)
    return;

  v8::HandleScope handle_scope(lpScriptContext->GetIsolate());
  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  pCJSRuntime->SetValueByNameInGlobalObject(
      szPropName, v8::Local<v8::Value>::New(lpScriptContext->GetIsolate(),
                                            pValue->DirectGetValue()));
}

// static
void CFXJSE_Engine::GlobalPropertyGetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pObject,
                                         ByteStringView szPropName,
                                         CFXJSE_Value* pValue) {
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pObject);
  CXFA_Document* pDoc = pOriginalObject->GetDocument();
  CFXJSE_Engine* lpScriptContext = pDoc->GetScriptContext();
  WideString wsPropName = WideString::FromUTF8(szPropName);

  pValue->SetUndefined(pIsolate);  // Assume failure.
  if (lpScriptContext->GetType() == CXFA_Script::Type::Formcalc) {
    if (szPropName == kFormCalcRuntime) {
      lpScriptContext->m_FM2JSContext->GlobalPropertyGetter(pValue);
      return;
    }
    XFA_HashCode uHashCode = static_cast<XFA_HashCode>(
        FX_HashCode_GetW(wsPropName.AsStringView(), false));
    if (uHashCode != XFA_HASHCODE_Layout) {
      CXFA_Object* pObj =
          lpScriptContext->GetDocument()->GetXFAObject(uHashCode);
      if (pObj) {
        pValue->ForceSetValue(
            pIsolate, lpScriptContext->GetOrCreateJSBindingFromMap(pObj));
        return;
      }
    }
  }

  CXFA_Node* pRefNode = ToNode(lpScriptContext->GetThisObject());
  if (pOriginalObject->IsThisProxy()) {
    pRefNode = ToNode(lpScriptContext->GetVariablesThis(pOriginalObject));
  }
  if (lpScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), pValue,
          XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Properties |
              XFA_RESOLVENODE_Attributes,
          false)) {
    return;
  }
  if (lpScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), pValue,
          XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings, false)) {
    return;
  }

  CXFA_Object* pScriptObject =
      lpScriptContext->GetVariablesScript(pOriginalObject);
  if (pScriptObject && lpScriptContext->QueryVariableValue(
                           pScriptObject->AsNode(), szPropName, pValue, true)) {
    return;
  }

  CXFA_FFNotify* pNotify = pDoc->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  auto* pCJSRuntime = static_cast<CJS_Runtime*>(hDoc->GetIJSRuntime());
  if (!pCJSRuntime)
    return;

  v8::HandleScope handle_scope(lpScriptContext->GetIsolate());
  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  v8::Local<v8::Value> temp_value;
  if (!pCJSRuntime->GetValueByNameFromGlobalObject(szPropName, &temp_value))
    return;

  if (temp_value.IsEmpty())
    return;

  pValue->ForceSetValue(pIsolate, temp_value);
}

// static
int32_t CFXJSE_Engine::GlobalPropTypeGetter(v8::Isolate* pIsolate,
                                            v8::Local<v8::Object> pHolder,
                                            ByteStringView szPropName,
                                            bool bQueryIn) {
  CXFA_Object* pObject = ToObject(pIsolate, pHolder);
  if (!pObject)
    return FXJSE_ClassPropType_None;

  CFXJSE_Engine* lpScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = lpScriptContext->GetVariablesThis(pObject);
  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (pObject->JSObject()->HasMethod(wsPropName))
    return FXJSE_ClassPropType_Method;

  return FXJSE_ClassPropType_Property;
}

// static
void CFXJSE_Engine::NormalPropertyGetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pHolder,
                                         ByteStringView szPropName,
                                         CFXJSE_Value* pReturnValue) {
  pReturnValue->SetUndefined(pIsolate);  // Assume failure.
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pHolder);
  if (!pOriginalObject)
    return;

  WideString wsPropName = WideString::FromUTF8(szPropName);
  CFXJSE_Engine* lpScriptContext =
      pOriginalObject->GetDocument()->GetScriptContext();
  CXFA_Object* pObject = lpScriptContext->GetVariablesThis(pOriginalObject);
  if (wsPropName.EqualsASCII("xfa")) {
    pReturnValue->ForceSetValue(pIsolate,
                                lpScriptContext->GetOrCreateJSBindingFromMap(
                                    lpScriptContext->GetDocument()->GetRoot()));
    return;
  }

  bool bRet = lpScriptContext->QueryNodeByFlag(
      ToNode(pObject), wsPropName.AsStringView(), pReturnValue,
      XFA_RESOLVENODE_Children | XFA_RESOLVENODE_Properties |
          XFA_RESOLVENODE_Attributes,
      false);
  if (bRet)
    return;

  if (pObject == lpScriptContext->GetThisObject() ||
      (lpScriptContext->GetType() == CXFA_Script::Type::Javascript &&
       !lpScriptContext->IsStrictScopeInJavaScript())) {
    bRet = lpScriptContext->QueryNodeByFlag(
        ToNode(pObject), wsPropName.AsStringView(), pReturnValue,
        XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings, false);
  }
  if (bRet)
    return;

  CXFA_Object* pScriptObject =
      lpScriptContext->GetVariablesScript(pOriginalObject);
  if (!pScriptObject)
    return;

  bRet = lpScriptContext->QueryVariableValue(ToNode(pScriptObject), szPropName,
                                             pReturnValue, true);
  if (bRet)
    return;

  Optional<XFA_SCRIPTATTRIBUTEINFO> info = XFA_GetScriptAttributeByName(
      pObject->GetElementType(), wsPropName.AsStringView());
  if (info.has_value()) {
    CJX_Object* jsObject = pObject->JSObject();
    (*info.value().callback)(pIsolate, jsObject, pReturnValue, false,
                             info.value().attribute);
    return;
  }

  CXFA_FFNotify* pNotify = pObject->GetDocument()->GetNotify();
  if (!pNotify)
    return;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  auto* pCJSRuntime = static_cast<CJS_Runtime*>(hDoc->GetIJSRuntime());
  if (!pCJSRuntime)
    return;

  v8::HandleScope handle_scope(lpScriptContext->GetIsolate());
  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  v8::Local<v8::Value> temp_local;
  if (!pCJSRuntime->GetValueByNameFromGlobalObject(szPropName, &temp_local))
    return;

  if (temp_local.IsEmpty())
    return;

  pReturnValue->ForceSetValue(lpScriptContext->GetIsolate(), temp_local);
}

// static
void CFXJSE_Engine::NormalPropertySetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pHolder,
                                         ByteStringView szPropName,
                                         CFXJSE_Value* pReturnValue) {
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pHolder);
  if (!pOriginalObject)
    return;

  CFXJSE_Engine* lpScriptContext =
      pOriginalObject->GetDocument()->GetScriptContext();
  CXFA_Object* pObject = lpScriptContext->GetVariablesThis(pOriginalObject);
  WideString wsPropName = WideString::FromUTF8(szPropName);
  WideStringView wsPropNameView = wsPropName.AsStringView();
  Optional<XFA_SCRIPTATTRIBUTEINFO> info =
      XFA_GetScriptAttributeByName(pObject->GetElementType(), wsPropNameView);
  if (info.has_value()) {
    CJX_Object* jsObject = pObject->JSObject();
    (*info.value().callback)(pIsolate, jsObject, pReturnValue, true,
                             info.value().attribute);
    return;
  }

  if (pObject->IsNode()) {
    if (wsPropNameView[0] == '#')
      wsPropNameView = wsPropNameView.Last(wsPropNameView.GetLength() - 1);

    CXFA_Node* pNode = ToNode(pObject);
    CXFA_Node* pPropOrChild = nullptr;
    XFA_Element eType = XFA_GetElementByName(wsPropNameView);
    if (eType != XFA_Element::Unknown) {
      pPropOrChild =
          pNode->JSObject()->GetOrCreateProperty<CXFA_Node>(0, eType);
    } else {
      pPropOrChild = pNode->GetFirstChildByName(wsPropNameView);
    }

    if (pPropOrChild) {
      info = XFA_GetScriptAttributeByName(pPropOrChild->GetElementType(),
                                          L"{default}");
      if (info.has_value()) {
        pPropOrChild->JSObject()->ScriptSomDefaultValue(
            pIsolate, pReturnValue, true, XFA_Attribute::Unknown);
        return;
      }
    }
  }

  CXFA_Object* pScriptObject =
      lpScriptContext->GetVariablesScript(pOriginalObject);
  if (pScriptObject) {
    lpScriptContext->QueryVariableValue(ToNode(pScriptObject), szPropName,
                                        pReturnValue, false);
  }
}

int32_t CFXJSE_Engine::NormalPropTypeGetter(v8::Isolate* pIsolate,
                                            v8::Local<v8::Object> pHolder,
                                            ByteStringView szPropName,
                                            bool bQueryIn) {
  CXFA_Object* pObject = ToObject(pIsolate, pHolder);
  if (!pObject)
    return FXJSE_ClassPropType_None;

  CFXJSE_Engine* lpScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = lpScriptContext->GetVariablesThis(pObject);
  XFA_Element eType = pObject->GetElementType();
  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (pObject->JSObject()->HasMethod(wsPropName))
    return FXJSE_ClassPropType_Method;

  if (bQueryIn &&
      !XFA_GetScriptAttributeByName(eType, wsPropName.AsStringView())) {
    return FXJSE_ClassPropType_None;
  }
  return FXJSE_ClassPropType_Property;
}

CJS_Result CFXJSE_Engine::NormalMethodCall(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const WideString& functionName) {
  CXFA_Object* pObject = ToObject(info);
  if (!pObject)
    return CJS_Result::Failure(L"no Holder() present.");

  CFXJSE_Engine* lpScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = lpScriptContext->GetVariablesThis(pObject);

  std::vector<v8::Local<v8::Value>> parameters;
  for (int i = 0; i < info.Length(); i++)
    parameters.push_back(info[i]);

  return pObject->JSObject()->RunMethod(functionName, parameters);
}

bool CFXJSE_Engine::IsStrictScopeInJavaScript() {
  return m_pDocument->is_strict_scoping();
}

CXFA_Script::Type CFXJSE_Engine::GetType() {
  return m_eScriptType;
}

CFXJSE_Context* CFXJSE_Engine::CreateVariablesContext(CXFA_Node* pScriptNode,
                                                      CXFA_Node* pSubform) {
  if (!pScriptNode || !pSubform)
    return nullptr;

  auto* proxy = cppgc::MakeGarbageCollected<CXFA_ThisProxy>(
      pScriptNode->GetDocument()->GetHeap()->GetAllocationHandle(), pSubform,
      pScriptNode);
  auto pNewContext = CFXJSE_Context::Create(
      GetIsolate(), &VariablesClassDescriptor, proxy->JSObject(), proxy);
  RemoveBuiltInObjs(pNewContext.get());
  pNewContext->EnableCompatibleMode();
  CFXJSE_Context* pResult = pNewContext.get();
  m_mapVariableToContext[pScriptNode->JSObject()] = std::move(pNewContext);
  return pResult;
}

CXFA_Object* CFXJSE_Engine::GetVariablesThis(CXFA_Object* pObject) {
  CXFA_ThisProxy* pProxy = ToThisProxy(pObject);
  return pProxy ? pProxy->GetThisNode() : pObject;
}

CXFA_Object* CFXJSE_Engine::GetVariablesScript(CXFA_Object* pObject) {
  CXFA_ThisProxy* pProxy = ToThisProxy(pObject);
  return pProxy ? pProxy->GetScriptNode() : pObject;
}

bool CFXJSE_Engine::RunVariablesScript(CXFA_Node* pScriptNode) {
  if (!pScriptNode)
    return false;

  if (pScriptNode->GetElementType() != XFA_Element::Script)
    return true;

  CXFA_Node* pParent = pScriptNode->GetParent();
  if (!pParent || pParent->GetElementType() != XFA_Element::Variables)
    return false;

  auto it = m_mapVariableToContext.find(pScriptNode->JSObject());
  if (it != m_mapVariableToContext.end() && it->second)
    return true;

  CXFA_Node* pTextNode = pScriptNode->GetFirstChild();
  if (!pTextNode)
    return false;

  Optional<WideString> wsScript =
      pTextNode->JSObject()->TryCData(XFA_Attribute::Value, true);
  if (!wsScript)
    return false;

  ByteString btScript = wsScript->ToUTF8();
  auto hRetValue = std::make_unique<CFXJSE_Value>();
  CXFA_Node* pThisObject = pParent->GetParent();
  CFXJSE_Context* pVariablesContext =
      CreateVariablesContext(pScriptNode, pThisObject);
  AutoRestorer<cppgc::Persistent<CXFA_Object>> nodeRestorer(&m_pThisObject);
  m_pThisObject = pThisObject;
  return pVariablesContext->ExecuteScript(btScript.c_str(), hRetValue.get(),
                                          v8::Local<v8::Object>());
}

bool CFXJSE_Engine::QueryVariableValue(CXFA_Node* pScriptNode,
                                       ByteStringView szPropName,
                                       CFXJSE_Value* pValue,
                                       bool bGetter) {
  if (!pScriptNode || pScriptNode->GetElementType() != XFA_Element::Script)
    return false;

  CXFA_Node* variablesNode = pScriptNode->GetParent();
  if (!variablesNode ||
      variablesNode->GetElementType() != XFA_Element::Variables) {
    return false;
  }

  auto it = m_mapVariableToContext.find(pScriptNode->JSObject());
  if (it == m_mapVariableToContext.end() || !it->second)
    return false;

  CFXJSE_Context* pVariableContext = it->second.get();
  v8::Local<v8::Object> pObject = pVariableContext->GetGlobalObject();
  if (!bGetter) {
    fxv8::ReentrantSetObjectOwnPropertyHelper(GetIsolate(), pObject, szPropName,
                                              pValue->GetValue(GetIsolate()));
    return true;
  }

  if (!fxv8::ReentrantHasObjectOwnPropertyHelper(GetIsolate(), pObject,
                                                 szPropName, false)) {
    return false;
  }

  v8::Local<v8::Value> hVariableValue =
      fxv8::ReentrantGetObjectPropertyHelper(GetIsolate(), pObject, szPropName);
  if (fxv8::IsFunction(hVariableValue)) {
    pValue->SetBoundFunction(GetIsolate(), hVariableValue.As<v8::Function>(),
                             pObject);
  } else {
    pValue->ForceSetValue(GetIsolate(), hVariableValue);
  }
  return true;
}

void CFXJSE_Engine::RemoveBuiltInObjs(CFXJSE_Context* pContext) {
  CFXJSE_ScopeUtil_IsolateHandleContext scope(this);
  v8::Local<v8::Object> pObject = pContext->GetGlobalObject();
  fxv8::ReentrantDeleteObjectPropertyHelper(GetIsolate(), pObject, "Number");
  fxv8::ReentrantDeleteObjectPropertyHelper(GetIsolate(), pObject, "Date");
}

bool CFXJSE_Engine::ResolveObjects(CXFA_Object* refObject,
                                   WideStringView wsExpression,
                                   XFA_ResolveNodeRS* resolveNodeRS,
                                   uint32_t dwStyles,
                                   CXFA_Node* bindNode) {
  if (wsExpression.IsEmpty())
    return false;

  if (m_eScriptType != CXFA_Script::Type::Formcalc ||
      (dwStyles & (XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings))) {
    m_upObjectArray.clear();
  }
  if (refObject && refObject->IsNode() &&
      (dwStyles & (XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings))) {
    m_upObjectArray.push_back(refObject->AsNode());
  }

  bool bNextCreate = false;
  CFXJSE_NodeHelper* pNodeHelper = m_ResolveProcessor->GetNodeHelper();
  if (dwStyles & XFA_RESOLVENODE_CreateNode)
    pNodeHelper->SetCreateNodeType(bindNode);

  pNodeHelper->m_pCreateParent = nullptr;
  pNodeHelper->m_iCurAllStart = -1;

  CFXJSE_ResolveNodeData rndFind(this);
  int32_t nStart = 0;
  int32_t nLevel = 0;

  std::vector<cppgc::Member<CXFA_Object>> findObjects;
  findObjects.emplace_back(refObject ? refObject : m_pDocument->GetRoot());
  int32_t nNodes = 0;
  while (true) {
    nNodes = pdfium::CollectionSize<int32_t>(findObjects);
    int32_t i = 0;
    rndFind.m_dwStyles = dwStyles;
    m_ResolveProcessor->SetCurStart(nStart);
    nStart = m_ResolveProcessor->GetFilter(wsExpression, nStart, rndFind);
    if (nStart < 1) {
      if ((dwStyles & XFA_RESOLVENODE_CreateNode) && !bNextCreate) {
        CXFA_Node* pDataNode = nullptr;
        nStart = pNodeHelper->m_iCurAllStart;
        if (nStart != -1) {
          pDataNode = m_pDocument->GetNotBindNode(findObjects);
          if (pDataNode) {
            findObjects.clear();
            findObjects.emplace_back(pDataNode);
            break;
          }
        } else {
          pDataNode = findObjects.front()->AsNode();
          findObjects.clear();
          findObjects.emplace_back(pDataNode);
          break;
        }
        dwStyles |= XFA_RESOLVENODE_Bind;
        findObjects.clear();
        findObjects.emplace_back(pNodeHelper->m_pAllStartParent.Get());
        continue;
      }
      break;
    }
    if (bNextCreate) {
      int32_t checked_length =
          pdfium::base::checked_cast<int32_t>(wsExpression.GetLength());
      if (pNodeHelper->CreateNode(rndFind.m_wsName, rndFind.m_wsCondition,
                                  nStart == checked_length, this)) {
        continue;
      }
      break;
    }
    std::vector<cppgc::Member<CXFA_Object>> retObjects;
    while (i < nNodes) {
      bool bDataBind = false;
      if (((dwStyles & XFA_RESOLVENODE_Bind) ||
           (dwStyles & XFA_RESOLVENODE_CreateNode)) &&
          nNodes > 1) {
        CFXJSE_ResolveNodeData rndBind(nullptr);
        m_ResolveProcessor->GetFilter(wsExpression, nStart, rndBind);
        m_ResolveProcessor->SetIndexDataBind(rndBind.m_wsCondition, i, nNodes);
        bDataBind = true;
      }
      rndFind.m_CurObject = findObjects[i++].Get();
      rndFind.m_nLevel = nLevel;
      rndFind.m_dwFlag = XFA_ResolveNodeRS::Type::kNodes;
      if (!m_ResolveProcessor->Resolve(GetIsolate(), rndFind))
        continue;

      if (rndFind.m_dwFlag == XFA_ResolveNodeRS::Type::kAttribute &&
          rndFind.m_ScriptAttribute.callback &&
          nStart <
              pdfium::base::checked_cast<int32_t>(wsExpression.GetLength())) {
        auto pValue = std::make_unique<CFXJSE_Value>();
        CJX_Object* jsObject = rndFind.m_Objects.front()->JSObject();
        (*rndFind.m_ScriptAttribute.callback)(
            GetIsolate(), jsObject, pValue.get(), false,
            rndFind.m_ScriptAttribute.attribute);
        if (!pValue->IsEmpty())
          rndFind.m_Objects.front() = ToObject(GetIsolate(), pValue.get());
      }
      if (!m_upObjectArray.empty())
        m_upObjectArray.pop_back();
      retObjects.insert(retObjects.end(), rndFind.m_Objects.begin(),
                        rndFind.m_Objects.end());
      rndFind.m_Objects.clear();
      if (bDataBind)
        break;
    }
    findObjects.clear();

    nNodes = pdfium::CollectionSize<int32_t>(retObjects);
    if (nNodes < 1) {
      if (dwStyles & XFA_RESOLVENODE_CreateNode) {
        bNextCreate = true;
        if (!pNodeHelper->m_pCreateParent) {
          pNodeHelper->m_pCreateParent = ToNode(rndFind.m_CurObject.Get());
          pNodeHelper->m_iCreateCount = 1;
        }
        int32_t checked_length =
            pdfium::base::checked_cast<int32_t>(wsExpression.GetLength());
        if (pNodeHelper->CreateNode(rndFind.m_wsName, rndFind.m_wsCondition,
                                    nStart == checked_length, this)) {
          continue;
        }
      }
      break;
    }

    findObjects = std::move(retObjects);
    rndFind.m_Objects.clear();
    if (nLevel == 0)
      dwStyles &= ~(XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings);

    nLevel++;
  }

  if (!bNextCreate) {
    resolveNodeRS->dwFlags = rndFind.m_dwFlag;
    if (nNodes > 0) {
      resolveNodeRS->objects.insert(resolveNodeRS->objects.end(),
                                    findObjects.begin(), findObjects.end());
    }
    if (rndFind.m_dwFlag == XFA_ResolveNodeRS::Type::kAttribute) {
      resolveNodeRS->script_attribute = rndFind.m_ScriptAttribute;
      return true;
    }
  }
  if (dwStyles & (XFA_RESOLVENODE_CreateNode | XFA_RESOLVENODE_Bind |
                  XFA_RESOLVENODE_BindNew)) {
    if (pNodeHelper->m_pCreateParent)
      resolveNodeRS->objects.emplace_back(pNodeHelper->m_pCreateParent.Get());
    else
      pNodeHelper->CreateNodeForCondition(rndFind.m_wsCondition);

    resolveNodeRS->dwFlags = pNodeHelper->m_iCreateFlag;
    if (resolveNodeRS->dwFlags == XFA_ResolveNodeRS::Type::kCreateNodeOne) {
      if (pNodeHelper->m_iCurAllStart != -1)
        resolveNodeRS->dwFlags = XFA_ResolveNodeRS::Type::kCreateNodeMidAll;
    }

    if (!bNextCreate && (dwStyles & XFA_RESOLVENODE_CreateNode))
      resolveNodeRS->dwFlags = XFA_ResolveNodeRS::Type::kExistNodes;

    return !resolveNodeRS->objects.empty();
  }
  return nNodes > 0;
}

v8::Local<v8::Object> CFXJSE_Engine::GetOrCreateJSBindingFromMap(
    CXFA_Object* pObject) {
  if (pObject->IsNode())
    RunVariablesScript(pObject->AsNode());

  CJX_Object* pCJXObject = pObject->JSObject();
  auto iter = m_mapObjectToObject.find(pCJXObject);
  if (iter != m_mapObjectToObject.end())
    return v8::Local<v8::Object>::New(GetIsolate(), iter->second);

  v8::Local<v8::Object> binding = pCJXObject->NewBoundV8Object(
      GetIsolate(), m_pJsClass->GetTemplate(GetIsolate()));

  m_mapObjectToObject[pCJXObject].Reset(GetIsolate(), binding);
  return binding;
}

void CFXJSE_Engine::SetNodesOfRunScript(
    std::vector<cppgc::Persistent<CXFA_Node>>* pArray) {
  m_pScriptNodeArray = pArray;
}

void CFXJSE_Engine::AddNodesOfRunScript(CXFA_Node* pNode) {
  if (m_pScriptNodeArray && !pdfium::Contains(*m_pScriptNodeArray, pNode))
    m_pScriptNodeArray->emplace_back(pNode);
}

CXFA_Object* CFXJSE_Engine::ToXFAObject(v8::Local<v8::Value> obj) {
  if (!fxv8::IsObject(obj))
    return nullptr;

  CFXJSE_HostObject* pHostObj =
      FXJSE_RetrieveObjectBinding(obj.As<v8::Object>());
  if (!pHostObj)
    return nullptr;

  CJX_Object* pJSObject = pHostObj->AsCJXObject();
  return pJSObject ? pJSObject->GetXFAObject() : nullptr;
}

v8::Local<v8::Object> CFXJSE_Engine::NewNormalXFAObject(CXFA_Object* obj) {
  v8::EscapableHandleScope scope(GetIsolate());
  v8::Local<v8::Object> object = obj->JSObject()->NewBoundV8Object(
      GetIsolate(), GetJseNormalClass()->GetTemplate(GetIsolate()));
  return scope.Escape(object);
}
