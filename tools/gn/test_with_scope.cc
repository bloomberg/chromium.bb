// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/test_with_scope.h"

#include "base/bind.h"
#include "tools/gn/parser.h"
#include "tools/gn/tokenizer.h"

TestWithScope::TestWithScope()
    : build_settings_(),
      settings_(&build_settings_, std::string()),
      toolchain_(&settings_, Label(SourceDir("//toolchain/"), "default")),
      scope_(&settings_) {
  build_settings_.SetBuildDir(SourceDir("//out/Debug/"));
  build_settings_.set_print_callback(
      base::Bind(&TestWithScope::AppendPrintOutput, base::Unretained(this)));

  settings_.set_toolchain_label(toolchain_.label());
  settings_.set_default_toolchain_label(toolchain_.label());
}

TestWithScope::~TestWithScope() {
}

void TestWithScope::AppendPrintOutput(const std::string& str) {
  print_output_.append(str);
}

TestParseInput::TestParseInput(const std::string& input)
    : input_file_(SourceFile("//test")) {
  input_file_.SetContents(input);

  tokens_ = Tokenizer::Tokenize(&input_file_, &parse_err_);
  if (!parse_err_.has_error())
    parsed_ = Parser::Parse(tokens_, &parse_err_);
}

TestParseInput::~TestParseInput() {
}
