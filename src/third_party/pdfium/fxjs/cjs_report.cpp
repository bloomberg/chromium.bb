// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cjs_report.h"

#include <vector>

#include "fxjs/cjs_object.h"
#include "fxjs/js_define.h"

const JSMethodSpec CJS_Report::MethodSpecs[] = {
    {"save", save_static},
    {"writeText", writeText_static}};

int CJS_Report::ObjDefnID = -1;
const char CJS_Report::kName[] = "Report";

// static
int CJS_Report::GetObjDefnID() {
  return ObjDefnID;
}

// static
void CJS_Report::DefineJSObjects(CFXJS_Engine* pEngine, FXJSOBJTYPE eObjType) {
  ObjDefnID = pEngine->DefineObj(CJS_Report::kName, eObjType,
                                 JSConstructor<CJS_Report>, JSDestructor);
  DefineMethods(pEngine, ObjDefnID, MethodSpecs);
}

CJS_Report::CJS_Report(v8::Local<v8::Object> pObject, CJS_Runtime* pRuntime)
    : CJS_Object(pObject, pRuntime) {}

CJS_Report::~CJS_Report() = default;

CJS_Result CJS_Report::writeText(
    CJS_Runtime* pRuntime,
    const std::vector<v8::Local<v8::Value>>& params) {
  // Unsafe, not supported, but do not return error.
  return CJS_Result::Success();
}

CJS_Result CJS_Report::save(CJS_Runtime* pRuntime,
                            const std::vector<v8::Local<v8::Value>>& params) {
  // Unsafe, not supported, but do not return error.
  return CJS_Result::Success();
}
