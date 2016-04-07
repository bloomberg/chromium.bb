// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintDefinition.h"

#include "bindings/core/v8/ScriptState.h"

namespace blink {

CSSPaintDefinition* CSSPaintDefinition::create(ScriptState* scriptState, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint)
{
    return new CSSPaintDefinition(scriptState, constructor, paint);
}

CSSPaintDefinition::CSSPaintDefinition(ScriptState* scriptState, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint)
    : m_scriptState(scriptState)
    , m_constructor(scriptState->isolate(), constructor)
    , m_paint(scriptState->isolate(), paint)
{
}

CSSPaintDefinition::~CSSPaintDefinition()
{
}

} // namespace blink
