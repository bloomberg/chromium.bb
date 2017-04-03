// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPaintImageGenerator.h"

namespace blink {

namespace {

CSSPaintImageGenerator::CSSPaintImageGeneratorCreateFunction s_createFunction =
    nullptr;

}  // namespace

// static
void CSSPaintImageGenerator::init(
    CSSPaintImageGeneratorCreateFunction createFunction) {
  DCHECK(!s_createFunction);
  s_createFunction = createFunction;
}

// static
CSSPaintImageGenerator* CSSPaintImageGenerator::create(const String& name,
                                                       Document& document,
                                                       Observer* observer) {
  DCHECK(s_createFunction);
  return s_createFunction(name, document, observer);
}

CSSPaintImageGenerator::~CSSPaintImageGenerator() {}

}  // namespace blink
