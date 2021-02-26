// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/external_engine_embedder_test.h"

#include "fxjs/cfxjs_engine.h"
#include "testing/v8_test_environment.h"

ExternalEngineEmbedderTest::ExternalEngineEmbedderTest() = default;

ExternalEngineEmbedderTest::~ExternalEngineEmbedderTest() = default;

void ExternalEngineEmbedderTest::SetUp() {
  EmbedderTest::SetUp();

  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  FXJS_PerIsolateData::SetUp(isolate());
  m_Engine = std::make_unique<CFXJS_Engine>(isolate());
  m_Engine->InitializeEngine();
}

void ExternalEngineEmbedderTest::TearDown() {
  m_Engine->ReleaseEngine();
  m_Engine.reset();
  JSEmbedderTest::TearDown();
}

v8::Local<v8::Context> ExternalEngineEmbedderTest::GetV8Context() {
  return m_Engine->GetV8Context();
}
