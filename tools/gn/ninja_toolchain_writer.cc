// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_toolchain_writer.h"

#include <fstream>

#include "base/file_util.h"
#include "base/strings/stringize_macros.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/trace.h"

NinjaToolchainWriter::NinjaToolchainWriter(
    const Settings* settings,
    const Toolchain* toolchain,
    const std::vector<const Target*>& targets,
    std::ostream& out)
    : settings_(settings),
      toolchain_(toolchain),
      targets_(targets),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(), ESCAPE_NINJA),
      helper_(settings->build_settings()) {
}

NinjaToolchainWriter::~NinjaToolchainWriter() {
}

void NinjaToolchainWriter::Run() {
  WriteRules();
  WriteSubninjas();
}

// static
bool NinjaToolchainWriter::RunAndWriteFile(
    const Settings* settings,
    const Toolchain* toolchain,
    const std::vector<const Target*>& targets) {
  NinjaHelper helper(settings->build_settings());
  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForToolchain(settings).GetSourceFile(
          settings->build_settings())));
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, FilePathToUTF8(ninja_file));

  base::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaToolchainWriter gen(settings, toolchain, targets, file);
  gen.Run();
  return true;
}

void NinjaToolchainWriter::WriteRules() {
  std::string indent("  ");

  NinjaHelper helper(settings_->build_settings());
  std::string rule_prefix = helper.GetRulePrefix(settings_);

  for (int i = Toolchain::TYPE_NONE + 1; i < Toolchain::TYPE_NUMTYPES; i++) {
    Toolchain::ToolType tool_type = static_cast<Toolchain::ToolType>(i);
    const Toolchain::Tool& tool = toolchain_->GetTool(tool_type);
    if (tool.command.empty())
      continue;

    out_ << "rule " << rule_prefix << Toolchain::ToolTypeToName(tool_type)
         << std::endl;

    #define WRITE_ARG(name) \
      if (!tool.name.empty()) \
        out_ << indent << "  " STRINGIZE(name) " = " << tool.name << std::endl;
    WRITE_ARG(command);
    WRITE_ARG(depfile);
    WRITE_ARG(description);
    WRITE_ARG(pool);
    WRITE_ARG(restat);
    WRITE_ARG(rspfile);
    WRITE_ARG(rspfile_content);
    #undef WRITE_ARG

    // Deps is called "depsformat" in GN to avoid confusion with dependencies.
    if (!tool.depsformat.empty()) \
      out_ << indent << "  deps = " << tool.depsformat << std::endl;
  }
  out_ << std::endl;
}

void NinjaToolchainWriter::WriteSubninjas() {
  // Write subninja commands for each generated target.
  for (size_t i = 0; i < targets_.size(); i++) {
    OutputFile ninja_file = helper_.GetNinjaFileForTarget(targets_[i]);
    out_ << "subninja ";
    path_output_.WriteFile(out_, ninja_file);
    out_ << std::endl;
  }
  out_ << std::endl;
}
