// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CFXJSE_ENGINE_H_
#define FXJS_XFA_CFXJSE_ENGINE_H_

#include <map>
#include <memory>
#include <type_traits>
#include <vector>

#include "core/fxcrt/mask.h"
#include "core/fxcrt/unowned_ptr.h"
#include "fxjs/cfx_v8.h"
#include "v8/include/cppgc/persistent.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_script.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"

class CFXJSE_Class;
class CFXJSE_Context;
class CFXJSE_FormCalcContext;
class CFXJSE_HostObject;
class CFXJSE_ResolveProcessor;
class CFXJSE_Value;
class CJS_Runtime;

enum class XFA_ResolveFlag : uint16_t {
  kChildren = 1 << 0,
  kTagName = 1 << 1,
  kAttributes = 1 << 2,
  kProperties = 1 << 3,
  kSiblings = 1 << 5,
  kParent = 1 << 6,
  kAnyChild = 1 << 7,
  kALL = 1 << 8,
  kCreateNode = 1 << 10,
  kBind = 1 << 11,
  kBindNew = 1 << 12,
};

class CFXJSE_Engine final : public CFX_V8 {
 public:
  class ResolveResult {
    CPPGC_STACK_ALLOCATED();  // Allow raw/unowned pointers.

   public:
    enum class Type {
      kNodes = 0,
      kAttribute,
      kCreateNodeOne,
      kCreateNodeAll,
      kCreateNodeMidAll,
      kExistNodes,
    };

    ResolveResult();
    ResolveResult(const ResolveResult& that);
    ResolveResult& operator=(const ResolveResult& that);
    ~ResolveResult();

    Type type = Type::kNodes;
    XFA_SCRIPTATTRIBUTEINFO script_attribute = {};

    // Vector of Member would be correct for stack-based vectors, if
    // STL worked with cppgc.
    std::vector<cppgc::Member<CXFA_Object>> objects;
  };

  static CXFA_Object* ToObject(const v8::FunctionCallbackInfo<v8::Value>& info);
  static CXFA_Object* ToObject(v8::Isolate* pIsolate,
                               v8::Local<v8::Value> value);
  static CXFA_Object* ToObject(v8::Isolate* pIsolate, CFXJSE_Value* pValue);
  static CXFA_Object* ToObject(CFXJSE_HostObject* pHostObj);
  static v8::Local<v8::Value> GlobalPropertyGetter(
      v8::Isolate* pIsolate,
      v8::Local<v8::Object> pObject,
      ByteStringView szPropName);
  static void GlobalPropertySetter(v8::Isolate* pIsolate,
                                   v8::Local<v8::Object> pObject,
                                   ByteStringView szPropName,
                                   v8::Local<v8::Value> pValue);
  static v8::Local<v8::Value> NormalPropertyGetter(
      v8::Isolate* pIsolate,
      v8::Local<v8::Object> pObject,
      ByteStringView szPropName);
  static void NormalPropertySetter(v8::Isolate* pIsolate,
                                   v8::Local<v8::Object> pObject,
                                   ByteStringView szPropName,
                                   v8::Local<v8::Value> pValue);
  static CJS_Result NormalMethodCall(
      const v8::FunctionCallbackInfo<v8::Value>& info,
      const WideString& functionName);
  static FXJSE_ClassPropType NormalPropTypeGetter(v8::Isolate* pIsolate,
                                                  v8::Local<v8::Object> pObject,
                                                  ByteStringView szPropName,
                                                  bool bQueryIn);
  static FXJSE_ClassPropType GlobalPropTypeGetter(v8::Isolate* pIsolate,
                                                  v8::Local<v8::Object> pObject,
                                                  ByteStringView szPropName,
                                                  bool bQueryIn);

  CFXJSE_Engine(CXFA_Document* pDocument, CJS_Runtime* fxjs_runtime);
  ~CFXJSE_Engine() override;

  void SetEventParam(CXFA_EventParam* param) { m_eventParam = param; }
  CXFA_EventParam* GetEventParam() const { return m_eventParam.Get(); }
  bool RunScript(CXFA_Script::Type eScriptType,
                 WideStringView wsScript,
                 CFXJSE_Value* pRetValue,
                 CXFA_Object* pThisObject);

  absl::optional<ResolveResult> ResolveObjects(CXFA_Object* refObject,
                                               WideStringView wsExpression,
                                               Mask<XFA_ResolveFlag> dwStyles);

  absl::optional<ResolveResult> ResolveObjectsWithBindNode(
      CXFA_Object* refObject,
      WideStringView wsExpression,
      Mask<XFA_ResolveFlag> dwStyles,
      CXFA_Node* bindNode);

  v8::Local<v8::Object> GetOrCreateJSBindingFromMap(CXFA_Object* pObject);

  CXFA_Object* GetThisObject() const { return m_pThisObject; }
  CFXJSE_Class* GetJseNormalClass() const { return m_pJsClass.Get(); }
  CFXJSE_Context* GetJseContext() const { return m_JsContext.get(); }

  void SetNodesOfRunScript(std::vector<cppgc::Persistent<CXFA_Node>>* pArray);
  void AddNodesOfRunScript(CXFA_Node* pNode);

  void SetRunAtType(XFA_AttributeValue eRunAt) { m_eRunAtType = eRunAt; }
  bool IsRunAtClient() { return m_eRunAtType != XFA_AttributeValue::Server; }

  CXFA_Script::Type GetType();
  std::vector<cppgc::Persistent<CXFA_Node>>* GetUpObjectArray() {
    return &m_upObjectArray;
  }
  CXFA_Document* GetDocument() const { return m_pDocument.Get(); }

  CXFA_Object* ToXFAObject(v8::Local<v8::Value> obj);
  v8::Local<v8::Object> NewNormalXFAObject(CXFA_Object* obj);

 private:
  CFXJSE_Context* CreateVariablesContext(CXFA_Node* pScriptNode,
                                         CXFA_Node* pSubform);
  void RemoveBuiltInObjs(CFXJSE_Context* pContext);
  bool QueryNodeByFlag(CXFA_Node* refNode,
                       WideStringView propname,
                       v8::Local<v8::Value>* pValue,
                       Mask<XFA_ResolveFlag> dwFlag);
  bool UpdateNodeByFlag(CXFA_Node* refNode,
                        WideStringView propname,
                        v8::Local<v8::Value> pValue,
                        Mask<XFA_ResolveFlag> dwFlag);
  bool IsStrictScopeInJavaScript();
  CXFA_Object* GetVariablesThis(CXFA_Object* pObject);
  CXFA_Object* GetVariablesScript(CXFA_Object* pObject);
  CFXJSE_Context* VariablesContextForScriptNode(CXFA_Node* pScriptNode);
  bool QueryVariableValue(CXFA_Node* pScriptNode,
                          ByteStringView szPropName,
                          v8::Local<v8::Value>* pValue);
  bool UpdateVariableValue(CXFA_Node* pScriptNode,
                           ByteStringView szPropName,
                           v8::Local<v8::Value> pValue);
  bool RunVariablesScript(CXFA_Node* pScriptNode);

  UnownedPtr<CJS_Runtime> const m_pSubordinateRuntime;
  cppgc::WeakPersistent<CXFA_Document> const m_pDocument;
  std::unique_ptr<CFXJSE_Context> m_JsContext;
  UnownedPtr<CFXJSE_Class> m_pJsClass;
  CXFA_Script::Type m_eScriptType = CXFA_Script::Type::Unknown;
  // |m_mapObjectToValue| is what ensures the v8 object bound to a
  // CJX_Object remains valid for the lifetime of the engine.
  std::map<CJX_Object*, v8::Global<v8::Object>> m_mapObjectToObject;
  std::map<CJX_Object*, std::unique_ptr<CFXJSE_Context>> m_mapVariableToContext;
  UnownedPtr<CXFA_EventParam> m_eventParam;
  std::vector<cppgc::Persistent<CXFA_Node>> m_upObjectArray;
  UnownedPtr<std::vector<cppgc::Persistent<CXFA_Node>>> m_pScriptNodeArray;
  std::unique_ptr<CFXJSE_ResolveProcessor> const m_ResolveProcessor;
  std::unique_ptr<CFXJSE_FormCalcContext> m_FM2JSContext;
  cppgc::Persistent<CXFA_Object> m_pThisObject;
  XFA_AttributeValue m_eRunAtType = XFA_AttributeValue::Client;
};

#endif  //  FXJS_XFA_CFXJSE_ENGINE_H_
