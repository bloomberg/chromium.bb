// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cfxjse_engine.h"

#include <utility>

#include "core/fxcrt/autorestorer.h"
#include "core/fxcrt/cfx_widetextbuf.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/stl_util.h"
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
#include "third_party/base/containers/contains.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_localemgr.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_object.h"
#include "xfa/fxfa/parser/cxfa_thisproxy.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"
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

CFXJSE_Engine::ResolveResult::ResolveResult() = default;

CFXJSE_Engine::ResolveResult::ResolveResult(const ResolveResult& that) =
    default;

CFXJSE_Engine::ResolveResult& CFXJSE_Engine::ResolveResult::operator=(
    const ResolveResult& that) = default;

CFXJSE_Engine::ResolveResult::~ResolveResult() = default;

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
  CFXJSE_ScopeUtil_IsolateHandleContext scope(GetJseContext());
  for (const auto& pair : m_mapObjectToObject) {
    const v8::Global<v8::Object>& binding = pair.second;
    FXJSE_ClearObjectBinding(v8::Local<v8::Object>::New(GetIsolate(), binding));
  }
}

bool CFXJSE_Engine::RunScript(CXFA_Script::Type eScriptType,
                              WideStringView wsScript,
                              CFXJSE_Value* hRetValue,
                              CXFA_Object* pThisObject) {
  CFXJSE_ScopeUtil_IsolateHandleContext scope(GetJseContext());
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
                                    v8::Local<v8::Value>* pValue,
                                    Mask<XFA_ResolveFlag> dwFlag) {
  if (!refNode)
    return false;

  Optional<CFXJSE_Engine::ResolveResult> maybeResult =
      ResolveObjects(refNode, propname, dwFlag);
  if (!maybeResult.has_value())
    return false;

  if (maybeResult.value().type == ResolveResult::Type::kNodes) {
    *pValue =
        GetOrCreateJSBindingFromMap(maybeResult.value().objects.front().Get());
    return true;
  }
  if (maybeResult.value().type == ResolveResult::Type::kAttribute &&
      maybeResult.value().script_attribute.callback) {
    CJX_Object* jsObject = maybeResult.value().objects.front()->JSObject();
    (*maybeResult.value().script_attribute.callback)(
        GetIsolate(), jsObject, pValue, false,
        maybeResult.value().script_attribute.attribute);
  }
  return true;
}

bool CFXJSE_Engine::UpdateNodeByFlag(CXFA_Node* refNode,
                                     WideStringView propname,
                                     v8::Local<v8::Value> pValue,
                                     Mask<XFA_ResolveFlag> dwFlag) {
  if (!refNode)
    return false;

  Optional<CFXJSE_Engine::ResolveResult> maybeResult =
      ResolveObjects(refNode, propname, dwFlag);
  if (!maybeResult.has_value())
    return false;

  if (maybeResult.value().type == ResolveResult::Type::kAttribute &&
      maybeResult.value().script_attribute.callback) {
    CJX_Object* jsObject = maybeResult.value().objects.front()->JSObject();
    (*maybeResult.value().script_attribute.callback)(
        GetIsolate(), jsObject, &pValue, true,
        maybeResult.value().script_attribute.attribute);
  }
  return true;
}

// static
void CFXJSE_Engine::GlobalPropertySetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pObject,
                                         ByteStringView szPropName,
                                         v8::Local<v8::Value> pValue) {
  CXFA_Object* pOriginalNode = ToObject(pIsolate, pObject);
  CXFA_Document* pDoc = pOriginalNode->GetDocument();
  CFXJSE_Engine* pScriptContext = pDoc->GetScriptContext();
  CXFA_Node* pRefNode = ToNode(pScriptContext->GetThisObject());
  if (pOriginalNode->IsThisProxy())
    pRefNode = ToNode(pScriptContext->GetVariablesThis(pOriginalNode));

  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (pScriptContext->UpdateNodeByFlag(
          pRefNode, wsPropName.AsStringView(), pValue,
          Mask<XFA_ResolveFlag>{
              XFA_ResolveFlag::kParent, XFA_ResolveFlag::kSiblings,
              XFA_ResolveFlag::kChildren, XFA_ResolveFlag::kProperties,
              XFA_ResolveFlag::kAttributes})) {
    return;
  }
  if (pOriginalNode->IsThisProxy() && fxv8::IsUndefined(pValue)) {
    fxv8::ReentrantDeleteObjectPropertyHelper(pScriptContext->GetIsolate(),
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

  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  pCJSRuntime->SetValueByNameInGlobalObject(szPropName, pValue);
}

// static
v8::Local<v8::Value> CFXJSE_Engine::GlobalPropertyGetter(
    v8::Isolate* pIsolate,
    v8::Local<v8::Object> pObject,
    ByteStringView szPropName) {
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pObject);
  CXFA_Document* pDoc = pOriginalObject->GetDocument();
  CFXJSE_Engine* pScriptContext = pDoc->GetScriptContext();
  WideString wsPropName = WideString::FromUTF8(szPropName);

  // Assume failure.
  v8::Local<v8::Value> pValue = fxv8::NewUndefinedHelper(pIsolate);

  if (pScriptContext->GetType() == CXFA_Script::Type::Formcalc) {
    if (szPropName == kFormCalcRuntime)
      return pScriptContext->m_FM2JSContext->GlobalPropertyGetter();

    XFA_HashCode uHashCode =
        static_cast<XFA_HashCode>(FX_HashCode_GetW(wsPropName.AsStringView()));
    if (uHashCode != XFA_HASHCODE_Layout) {
      CXFA_Object* pObj =
          pScriptContext->GetDocument()->GetXFAObject(uHashCode);
      if (pObj)
        return pScriptContext->GetOrCreateJSBindingFromMap(pObj);
    }
  }

  CXFA_Node* pRefNode = ToNode(pScriptContext->GetThisObject());
  if (pOriginalObject->IsThisProxy())
    pRefNode = ToNode(pScriptContext->GetVariablesThis(pOriginalObject));

  if (pScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), &pValue,
          Mask<XFA_ResolveFlag>{XFA_ResolveFlag::kChildren,
                                XFA_ResolveFlag::kProperties,
                                XFA_ResolveFlag::kAttributes})) {
    return pValue;
  }
  if (pScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), &pValue,
          Mask<XFA_ResolveFlag>{XFA_ResolveFlag::kParent,
                                XFA_ResolveFlag::kSiblings})) {
    return pValue;
  }

  CXFA_Object* pScriptObject =
      pScriptContext->GetVariablesScript(pOriginalObject);
  if (pScriptObject && pScriptContext->QueryVariableValue(
                           pScriptObject->AsNode(), szPropName, &pValue)) {
    return pValue;
  }

  CXFA_FFNotify* pNotify = pDoc->GetNotify();
  if (!pNotify)
    return pValue;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  auto* pCJSRuntime = static_cast<CJS_Runtime*>(hDoc->GetIJSRuntime());
  if (!pCJSRuntime)
    return pValue;

  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  v8::Local<v8::Value> temp_value =
      pCJSRuntime->GetValueByNameFromGlobalObject(szPropName);

  return !temp_value.IsEmpty() ? temp_value : pValue;
}

// static
FXJSE_ClassPropType CFXJSE_Engine::GlobalPropTypeGetter(
    v8::Isolate* pIsolate,
    v8::Local<v8::Object> pHolder,
    ByteStringView szPropName,
    bool bQueryIn) {
  CXFA_Object* pObject = ToObject(pIsolate, pHolder);
  if (!pObject)
    return FXJSE_ClassPropType::kNone;

  CFXJSE_Engine* pScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = pScriptContext->GetVariablesThis(pObject);
  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (pObject->JSObject()->HasMethod(wsPropName))
    return FXJSE_ClassPropType::kMethod;

  return FXJSE_ClassPropType::kProperty;
}

// static
v8::Local<v8::Value> CFXJSE_Engine::NormalPropertyGetter(
    v8::Isolate* pIsolate,
    v8::Local<v8::Object> pHolder,
    ByteStringView szPropName) {
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pHolder);
  if (!pOriginalObject)
    return fxv8::NewUndefinedHelper(pIsolate);

  CFXJSE_Engine* pScriptContext =
      pOriginalObject->GetDocument()->GetScriptContext();

  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (wsPropName.EqualsASCII("xfa")) {
    return pScriptContext->GetOrCreateJSBindingFromMap(
        pScriptContext->GetDocument()->GetRoot());
  }

  v8::Local<v8::Value> pReturnValue = fxv8::NewUndefinedHelper(pIsolate);
  CXFA_Object* pObject = pScriptContext->GetVariablesThis(pOriginalObject);
  CXFA_Node* pRefNode = ToNode(pObject);
  if (pScriptContext->QueryNodeByFlag(
          pRefNode, wsPropName.AsStringView(), &pReturnValue,
          Mask<XFA_ResolveFlag>{XFA_ResolveFlag::kChildren,
                                XFA_ResolveFlag::kProperties,
                                XFA_ResolveFlag::kAttributes})) {
    return pReturnValue;
  }
  if (pObject == pScriptContext->GetThisObject() ||
      (pScriptContext->GetType() == CXFA_Script::Type::Javascript &&
       !pScriptContext->IsStrictScopeInJavaScript())) {
    if (pScriptContext->QueryNodeByFlag(
            pRefNode, wsPropName.AsStringView(), &pReturnValue,
            Mask<XFA_ResolveFlag>{XFA_ResolveFlag::kParent,
                                  XFA_ResolveFlag::kSiblings})) {
      return pReturnValue;
    }
  }
  CXFA_Object* pScriptObject =
      pScriptContext->GetVariablesScript(pOriginalObject);
  if (!pScriptObject)
    return pReturnValue;

  if (pScriptContext->QueryVariableValue(ToNode(pScriptObject), szPropName,
                                         &pReturnValue)) {
    return pReturnValue;
  }
  Optional<XFA_SCRIPTATTRIBUTEINFO> info = XFA_GetScriptAttributeByName(
      pObject->GetElementType(), wsPropName.AsStringView());
  if (info.has_value()) {
    (*info.value().callback)(pIsolate, pObject->JSObject(), &pReturnValue,
                             false, info.value().attribute);
    return pReturnValue;
  }

  CXFA_FFNotify* pNotify = pObject->GetDocument()->GetNotify();
  if (!pNotify)
    return pReturnValue;

  CXFA_FFDoc* hDoc = pNotify->GetFFDoc();
  auto* pCJSRuntime = static_cast<CJS_Runtime*>(hDoc->GetIJSRuntime());
  if (!pCJSRuntime)
    return pReturnValue;

  IJS_Runtime::ScopedEventContext pContext(pCJSRuntime);
  v8::Local<v8::Value> temp_local =
      pCJSRuntime->GetValueByNameFromGlobalObject(szPropName);

  return !temp_local.IsEmpty() ? temp_local : pReturnValue;
}

// static
void CFXJSE_Engine::NormalPropertySetter(v8::Isolate* pIsolate,
                                         v8::Local<v8::Object> pHolder,
                                         ByteStringView szPropName,
                                         v8::Local<v8::Value> pValue) {
  CXFA_Object* pOriginalObject = ToObject(pIsolate, pHolder);
  if (!pOriginalObject)
    return;

  CFXJSE_Engine* pScriptContext =
      pOriginalObject->GetDocument()->GetScriptContext();
  CXFA_Object* pObject = pScriptContext->GetVariablesThis(pOriginalObject);
  WideString wsPropName = WideString::FromUTF8(szPropName);
  WideStringView wsPropNameView = wsPropName.AsStringView();
  Optional<XFA_SCRIPTATTRIBUTEINFO> info =
      XFA_GetScriptAttributeByName(pObject->GetElementType(), wsPropNameView);
  if (info.has_value()) {
    CJX_Object* jsObject = pObject->JSObject();
    (*info.value().callback)(pIsolate, jsObject, &pValue, true,
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
        pPropOrChild->JSObject()->ScriptSomDefaultValue(pIsolate, &pValue, true,
                                                        XFA_Attribute::Unknown);
        return;
      }
    }
  }

  CXFA_Object* pScriptObject =
      pScriptContext->GetVariablesScript(pOriginalObject);
  if (pScriptObject) {
    pScriptContext->UpdateVariableValue(ToNode(pScriptObject), szPropName,
                                        pValue);
  }
}

FXJSE_ClassPropType CFXJSE_Engine::NormalPropTypeGetter(
    v8::Isolate* pIsolate,
    v8::Local<v8::Object> pHolder,
    ByteStringView szPropName,
    bool bQueryIn) {
  CXFA_Object* pObject = ToObject(pIsolate, pHolder);
  if (!pObject)
    return FXJSE_ClassPropType::kNone;

  CFXJSE_Engine* pScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = pScriptContext->GetVariablesThis(pObject);
  XFA_Element eType = pObject->GetElementType();
  WideString wsPropName = WideString::FromUTF8(szPropName);
  if (pObject->JSObject()->HasMethod(wsPropName))
    return FXJSE_ClassPropType::kMethod;

  if (bQueryIn) {
    Optional<XFA_SCRIPTATTRIBUTEINFO> maybe_info =
        XFA_GetScriptAttributeByName(eType, wsPropName.AsStringView());
    if (!maybe_info.has_value())
      return FXJSE_ClassPropType::kNone;
  }
  return FXJSE_ClassPropType::kProperty;
}

CJS_Result CFXJSE_Engine::NormalMethodCall(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const WideString& functionName) {
  CXFA_Object* pObject = ToObject(info);
  if (!pObject)
    return CJS_Result::Failure(L"no Holder() present.");

  CFXJSE_Engine* pScriptContext = pObject->GetDocument()->GetScriptContext();
  pObject = pScriptContext->GetVariablesThis(pObject);

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
  if (!wsScript.has_value())
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

CFXJSE_Context* CFXJSE_Engine::VariablesContextForScriptNode(
    CXFA_Node* pScriptNode) {
  if (!pScriptNode || pScriptNode->GetElementType() != XFA_Element::Script)
    return nullptr;

  CXFA_Node* variablesNode = pScriptNode->GetParent();
  if (!variablesNode ||
      variablesNode->GetElementType() != XFA_Element::Variables) {
    return nullptr;
  }

  auto it = m_mapVariableToContext.find(pScriptNode->JSObject());
  return it != m_mapVariableToContext.end() ? it->second.get() : nullptr;
}

bool CFXJSE_Engine::QueryVariableValue(CXFA_Node* pScriptNode,
                                       ByteStringView szPropName,
                                       v8::Local<v8::Value>* pValue) {
  CFXJSE_Context* pVariableContext = VariablesContextForScriptNode(pScriptNode);
  if (!pVariableContext)
    return false;

  v8::Local<v8::Object> pObject = pVariableContext->GetGlobalObject();
  if (!fxv8::ReentrantHasObjectOwnPropertyHelper(GetIsolate(), pObject,
                                                 szPropName)) {
    return false;
  }

  v8::Local<v8::Value> hVariableValue =
      fxv8::ReentrantGetObjectPropertyHelper(GetIsolate(), pObject, szPropName);
  if (fxv8::IsFunction(hVariableValue)) {
    v8::Local<v8::Function> maybeFunc = CFXJSE_Value::NewBoundFunction(
        GetIsolate(), hVariableValue.As<v8::Function>(), pObject);
    if (!maybeFunc.IsEmpty())
      *pValue = maybeFunc;
  } else {
    *pValue = hVariableValue;
  }
  return true;
}

bool CFXJSE_Engine::UpdateVariableValue(CXFA_Node* pScriptNode,
                                        ByteStringView szPropName,
                                        v8::Local<v8::Value> pValue) {
  CFXJSE_Context* pVariableContext = VariablesContextForScriptNode(pScriptNode);
  if (!pVariableContext)
    return false;

  v8::Local<v8::Object> pObject = pVariableContext->GetGlobalObject();
  fxv8::ReentrantSetObjectOwnPropertyHelper(GetIsolate(), pObject, szPropName,
                                            pValue);
  return true;
}

void CFXJSE_Engine::RemoveBuiltInObjs(CFXJSE_Context* pContext) {
  CFXJSE_ScopeUtil_IsolateHandleContext scope(GetJseContext());
  v8::Local<v8::Object> pObject = pContext->GetGlobalObject();
  fxv8::ReentrantDeleteObjectPropertyHelper(GetIsolate(), pObject, "Number");
  fxv8::ReentrantDeleteObjectPropertyHelper(GetIsolate(), pObject, "Date");
}

Optional<CFXJSE_Engine::ResolveResult> CFXJSE_Engine::ResolveObjects(
    CXFA_Object* refObject,
    WideStringView wsExpression,
    Mask<XFA_ResolveFlag> dwStyles) {
  return ResolveObjectsWithBindNode(refObject, wsExpression, dwStyles, nullptr);
}

Optional<CFXJSE_Engine::ResolveResult>
CFXJSE_Engine::ResolveObjectsWithBindNode(CXFA_Object* refObject,
                                          WideStringView wsExpression,
                                          Mask<XFA_ResolveFlag> dwStyles,
                                          CXFA_Node* bindNode) {
  if (wsExpression.IsEmpty())
    return pdfium::nullopt;

  const bool bParentOrSiblings =
      !!(dwStyles & Mask<XFA_ResolveFlag>{XFA_ResolveFlag::kParent,
                                          XFA_ResolveFlag::kSiblings});
  if (m_eScriptType != CXFA_Script::Type::Formcalc || bParentOrSiblings)
    m_upObjectArray.clear();
  if (refObject && refObject->IsNode() && bParentOrSiblings)
    m_upObjectArray.push_back(refObject->AsNode());

  ResolveResult result;
  bool bNextCreate = false;
  CFXJSE_NodeHelper* pNodeHelper = m_ResolveProcessor->GetNodeHelper();
  if (dwStyles & XFA_ResolveFlag::kCreateNode)
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
    nNodes = fxcrt::CollectionSize<int32_t>(findObjects);
    int32_t i = 0;
    rndFind.m_dwStyles = dwStyles;
    m_ResolveProcessor->SetCurStart(nStart);
    nStart = m_ResolveProcessor->GetFilter(wsExpression, nStart, rndFind);
    if (nStart < 1) {
      if ((dwStyles & XFA_ResolveFlag::kCreateNode) && !bNextCreate) {
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
        dwStyles |= XFA_ResolveFlag::kBind;
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
      if (((dwStyles & XFA_ResolveFlag::kBind) ||
           (dwStyles & XFA_ResolveFlag::kCreateNode)) &&
          nNodes > 1) {
        CFXJSE_ResolveNodeData rndBind(nullptr);
        m_ResolveProcessor->GetFilter(wsExpression, nStart, rndBind);
        i = m_ResolveProcessor->IndexForDataBind(rndBind.m_wsCondition, nNodes);
        bDataBind = true;
      }
      rndFind.m_CurObject = findObjects[i++].Get();
      rndFind.m_nLevel = nLevel;
      rndFind.m_Result.type = ResolveResult::Type::kNodes;
      if (!m_ResolveProcessor->Resolve(GetIsolate(), rndFind))
        continue;

      if (rndFind.m_Result.type == ResolveResult::Type::kAttribute &&
          rndFind.m_Result.script_attribute.callback &&
          nStart <
              pdfium::base::checked_cast<int32_t>(wsExpression.GetLength())) {
        v8::Local<v8::Value> pValue;
        CJX_Object* jsObject = rndFind.m_Result.objects.front()->JSObject();
        (*rndFind.m_Result.script_attribute.callback)(
            GetIsolate(), jsObject, &pValue, false,
            rndFind.m_Result.script_attribute.attribute);
        if (!pValue.IsEmpty()) {
          rndFind.m_Result.objects.front() = ToObject(GetIsolate(), pValue);
        }
      }
      if (!m_upObjectArray.empty())
        m_upObjectArray.pop_back();
      retObjects.insert(retObjects.end(), rndFind.m_Result.objects.begin(),
                        rndFind.m_Result.objects.end());
      rndFind.m_Result.objects.clear();
      if (bDataBind)
        break;
    }
    findObjects.clear();

    nNodes = fxcrt::CollectionSize<int32_t>(retObjects);
    if (nNodes < 1) {
      if (dwStyles & XFA_ResolveFlag::kCreateNode) {
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
    rndFind.m_Result.objects.clear();
    if (nLevel == 0) {
      dwStyles.Clear(XFA_ResolveFlag::kParent);
      dwStyles.Clear(XFA_ResolveFlag::kSiblings);
    }
    nLevel++;
  }

  if (!bNextCreate) {
    result.type = rndFind.m_Result.type;
    if (nNodes > 0) {
      result.objects.insert(result.objects.end(), findObjects.begin(),
                            findObjects.end());
    }
    if (rndFind.m_Result.type == ResolveResult::Type::kAttribute) {
      result.script_attribute = rndFind.m_Result.script_attribute;
      return result;
    }
  }
  if ((dwStyles & XFA_ResolveFlag::kCreateNode) ||
      (dwStyles & XFA_ResolveFlag::kBind) ||
      (dwStyles & XFA_ResolveFlag::kBindNew)) {
    if (pNodeHelper->m_pCreateParent)
      result.objects.emplace_back(pNodeHelper->m_pCreateParent.Get());
    else
      pNodeHelper->CreateNodeForCondition(rndFind.m_wsCondition);

    result.type = pNodeHelper->m_iCreateFlag;
    if (result.type == ResolveResult::Type::kCreateNodeOne) {
      if (pNodeHelper->m_iCurAllStart != -1)
        result.type = ResolveResult::Type::kCreateNodeMidAll;
    }

    if (!bNextCreate && (dwStyles & XFA_ResolveFlag::kCreateNode))
      result.type = ResolveResult::Type::kExistNodes;

    if (result.objects.empty())
      return pdfium::nullopt;

    return result;
  }
  if (nNodes == 0)
    return pdfium::nullopt;

  return result;
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
