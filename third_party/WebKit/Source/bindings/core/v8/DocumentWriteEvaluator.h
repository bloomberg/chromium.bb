// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentWriteEvaluator_h
#define DocumentWriteEvaluator_h

#include <memory>
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

// This class is used by the preload scanner on the background parser thread to
// execute inline Javascript and preload resources that would have been written
// by document.write(). It takes a script string and outputs a vector of start
// tag tokens.
class CORE_EXPORT DocumentWriteEvaluator {
  WTF_MAKE_NONCOPYABLE(DocumentWriteEvaluator);
  USING_FAST_MALLOC(DocumentWriteEvaluator);

 public:
  // For unit testing.
  DocumentWriteEvaluator(const String& path_name,
                         const String& host_name,
                         const String& protocol,
                         const String& user_agent);

  static std::unique_ptr<DocumentWriteEvaluator> Create(
      const Document& document) {
    return WTF::WrapUnique(new DocumentWriteEvaluator(document));
  }
  virtual ~DocumentWriteEvaluator();

  // Initializes the V8 context for this document. Returns whether
  // initialization was needed.
  bool EnsureEvaluationContext();
  String EvaluateAndEmitWrittenSource(const String& script_source);
  bool ShouldEvaluate(const String& script_source);

  void RecordDocumentWrite(const String& document_written_string);

 private:
  explicit DocumentWriteEvaluator(const Document&);
  // Returns true if the evaluation succeeded with no errors.
  bool Evaluate(const String& script_source);

  // All the strings that are document.written in the script tag that is being
  // scanned.
  StringBuilder document_written_strings_;

  ScopedPersistent<v8::Context> persistent_context_;
  ScopedPersistent<v8::Object> window_;
  ScopedPersistent<v8::Object> document_;
  ScopedPersistent<v8::Object> location_;
  ScopedPersistent<v8::Object> navigator_;

  String path_name_;
  String host_name_;
  String protocol_;
  String user_agent_;
};

}  // namespace blink

#endif  // DocumentWriteEvaluator_h
