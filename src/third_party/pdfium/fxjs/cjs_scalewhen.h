// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_CJS_SCALEWHEN_H_
#define FXJS_CJS_SCALEWHEN_H_

#include "fxjs/cjs_object.h"

class CJS_ScaleWhen final : public CJS_Object {
 public:
  static void DefineJSObjects(CFXJS_Engine* pEngine);

  CJS_ScaleWhen() = delete;

 private:
  static uint32_t ObjDefnID;
  static const JSConstSpec ConstSpecs[];
};

#endif  // FXJS_CJS_SCALEWHEN_H_
