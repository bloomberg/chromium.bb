// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/test_with_scope.h"

#include "base/bind.h"
#include "tools/gn/parser.h"
#include "tools/gn/tokenizer.h"

namespace {

void SetCommandForTool(const std::string& cmd, Tool* tool) {
  Err err;
  SubstitutionPattern command;
  command.Parse(cmd, NULL, &err);
  CHECK(!err.has_error())
      << "Couldn't parse \"" << cmd << "\", " << "got " << err.message();
  tool->set_command(command);
}

}  // namespace

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

  SetupToolchain(&toolchain_);
}

TestWithScope::~TestWithScope() {
}

// static
void TestWithScope::SetupToolchain(Toolchain* toolchain) {
  Err err;

  // CC
  scoped_ptr<Tool> cc_tool(new Tool);
  SetCommandForTool(
      "cc {{source}} {{cflags}} {{cflags_c}} {{defines}} {{include_dirs}} "
      "-o {{output}}",
      cc_tool.get());
  cc_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o"));
  toolchain->SetTool(Toolchain::TYPE_CC, cc_tool.Pass());

  // CXX
  scoped_ptr<Tool> cxx_tool(new Tool);
  SetCommandForTool(
      "c++ {{source}} {{cflags}} {{cflags_cc}} {{defines}} {{include_dirs}} "
      "-o {{output}}",
      cxx_tool.get());
  cxx_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o"));
  toolchain->SetTool(Toolchain::TYPE_CXX, cxx_tool.Pass());

  // OBJC
  scoped_ptr<Tool> objc_tool(new Tool);
  SetCommandForTool(
      "objcc {{source}} {{cflags}} {{cflags_objc}} {{defines}} "
      "{{include_dirs}} -o {{output}}",
      objc_tool.get());
  objc_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o"));
  toolchain->SetTool(Toolchain::TYPE_OBJC, objc_tool.Pass());

  // OBJC
  scoped_ptr<Tool> objcxx_tool(new Tool);
  SetCommandForTool(
      "objcxx {{source}} {{cflags}} {{cflags_objcc}} {{defines}} "
      "{{include_dirs}} -o {{output}}",
      objcxx_tool.get());
  objcxx_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o"));
  toolchain->SetTool(Toolchain::TYPE_OBJCXX, objcxx_tool.Pass());

  // Don't use RC and ASM tools in unit tests yet. Add here if needed.

  // ALINK
  scoped_ptr<Tool> alink_tool(new Tool);
  SetCommandForTool("ar {{output}} {{source}}", alink_tool.get());
  alink_tool->set_output_prefix("lib");
  alink_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{target_out_dir}}/{{target_output_name}}.a"));
  toolchain->SetTool(Toolchain::TYPE_ALINK, alink_tool.Pass());

  // SOLINK
  scoped_ptr<Tool> solink_tool(new Tool);
  SetCommandForTool("ld -shared -o {{target_output_name}}.so {{inputs}} "
      "{{ldflags}} {{libs}}", solink_tool.get());
  solink_tool->set_output_prefix("lib");
  solink_tool->set_default_output_extension(".so");
  solink_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{root_out_dir}}/{{target_output_name}}{{output_extension}}"));
  toolchain->SetTool(Toolchain::TYPE_SOLINK, solink_tool.Pass());

  // LINK
  scoped_ptr<Tool> link_tool(new Tool);
  SetCommandForTool("ld -o {{target_output_name}} {{source}} "
      "{{ldflags}} {{libs}}", link_tool.get());
  link_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{root_out_dir}}/{{target_output_name}}"));
  toolchain->SetTool(Toolchain::TYPE_LINK, link_tool.Pass());

  // STAMP
  scoped_ptr<Tool> stamp_tool(new Tool);
  SetCommandForTool("touch {{output}}", stamp_tool.get());
  toolchain->SetTool(Toolchain::TYPE_STAMP, stamp_tool.Pass());

  // COPY
  scoped_ptr<Tool> copy_tool(new Tool);
  SetCommandForTool("cp {{source}} {{output}}", copy_tool.get());
  toolchain->SetTool(Toolchain::TYPE_COPY, copy_tool.Pass());

  toolchain->ToolchainSetupComplete();
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
