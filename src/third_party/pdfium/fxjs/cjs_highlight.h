// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CJS_HIGHLIGHT_H_
#define FXJS_CJS_HIGHLIGHT_H_

#include "fxjs/js_define.h"

class CJS_Highlight final : public CJS_Object {
 public:
  static void DefineJSObjects(CFXJS_Engine* pEngine);

  CJS_Highlight(v8::Local<v8::Object> pObject, CJS_Runtime* pRuntime);
  ~CJS_Highlight() override;

 private:
  static int ObjDefnID;
  static const JSConstSpec ConstSpecs[];
};

#endif  // FXJS_CJS_HIGHLIGHT_H_
