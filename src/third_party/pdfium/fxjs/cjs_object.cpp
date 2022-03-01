// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cjs_object.h"

#include "fxjs/cfxjs_engine.h"

// static
void CJS_Object::DefineConsts(CFXJS_Engine* pEngine,
                              uint32_t nObjDefnID,
                              pdfium::span<const JSConstSpec> consts) {
  for (const auto& item : consts) {
    pEngine->DefineObjConst(
        nObjDefnID, item.pName,
        item.eType == JSConstSpec::Number
            ? pEngine->NewNumber(item.number).As<v8::Value>()
            : pEngine->NewString(item.pStr).As<v8::Value>());
  }
}

// static
void CJS_Object::DefineProps(CFXJS_Engine* pEngine,
                             uint32_t nObjDefnID,
                             pdfium::span<const JSPropertySpec> props) {
  for (const auto& item : props)
    pEngine->DefineObjProperty(nObjDefnID, item.pName, item.pPropGet,
                               item.pPropPut);
}

// static
void CJS_Object::DefineMethods(CFXJS_Engine* pEngine,
                               uint32_t nObjDefnID,
                               pdfium::span<const JSMethodSpec> methods) {
  for (const auto& item : methods)
    pEngine->DefineObjMethod(nObjDefnID, item.pName, item.pMethodCall);
}

CJS_Object::CJS_Object(v8::Local<v8::Object> pObject, CJS_Runtime* pRuntime)
    : m_pV8Object(pObject->GetIsolate(), pObject), m_pRuntime(pRuntime) {}

CJS_Object::~CJS_Object() = default;
