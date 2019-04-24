// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_XFA_JS_EMBEDDER_TEST_H_
#define TESTING_XFA_JS_EMBEDDER_TEST_H_

#include <memory>
#include <string>

#include "testing/embedder_test.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_node.h"

class CFXJSE_Engine;
class CFXJSE_Value;
class CFX_V8ArrayBufferAllocator;

class XFAJSEmbedderTest : public EmbedderTest {
 public:
  XFAJSEmbedderTest();
  ~XFAJSEmbedderTest() override;

  // EmbedderTest:
  void SetUp() override;
  void TearDown() override;
  bool OpenDocumentWithOptions(const std::string& filename,
                               const char* password,
                               LinearizeOption linearize_option,
                               JavaScriptOption javascript_option) override;

  v8::Isolate* GetIsolate() const { return isolate_; }
  CXFA_Document* GetXFADocument() const;

  bool Execute(ByteStringView input);
  bool ExecuteSilenceFailure(ByteStringView input);

  CFXJSE_Engine* GetScriptContext() const { return script_context_; }
  CFXJSE_Value* GetValue() const { return value_.get(); }

 private:
  std::unique_ptr<CFX_V8ArrayBufferAllocator> array_buffer_allocator_;
  std::unique_ptr<CFXJSE_Value> value_;
  v8::Isolate* isolate_ = nullptr;
  CFXJSE_Engine* script_context_ = nullptr;

  bool ExecuteHelper(ByteStringView input);
};

#endif  // TESTING_XFA_JS_EMBEDDER_TEST_H_
