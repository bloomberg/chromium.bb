// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

#include "base/compiler_specific.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/toolchain.h"

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaBinaryTargetWriter : public NinjaTargetWriter {
 public:
  NinjaBinaryTargetWriter(const Target* target,
                          const Toolchain* toolchain,
                          std::ostream& out);
  virtual ~NinjaBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  typedef std::set<OutputFile> OutputFileSet;

  void WriteCompilerVars();
  void WriteSources(std::vector<OutputFile>* object_files);
  void WriteLinkerStuff(const std::vector<OutputFile>& object_files);
  void WriteLinkerFlags(const Toolchain::Tool& tool,
                        const OutputFile& windows_manifest);
  void WriteLibs(const Toolchain::Tool& tool);

  // Writes the build line for linking the target. Includes newline.
  void WriteLinkCommand(const OutputFile& external_output_file,
                        const OutputFile& internal_output_file,
                        const std::vector<OutputFile>& object_files);

  // Writes the stamp line for a source set. These are not linked.
  void WriteSourceSetStamp(const std::vector<OutputFile>& object_files);

  // Gets all target dependencies and classifies them, as well as accumulates
  // object files from source sets we need to link.
  void GetDeps(std::set<OutputFile>* extra_object_files,
               std::vector<const Target*>* linkable_deps,
               std::vector<const Target*>* non_linkable_deps) const;

  // Classifies the dependency as linkable or nonlinkable with the current
  // target, adding it to the appropriate vector. If the dependency is a source
  // set we should link in, the source set's object files will be appended to
  // |extra_object_files|.
  void ClassifyDependency(const Target* dep,
                          std::set<OutputFile>* extra_object_files,
                          std::vector<const Target*>* linkable_deps,
                          std::vector<const Target*>* non_linkable_deps) const;

  // Writes the implicit dependencies for the link or stamp line. This is
  // the "||" and everything following it on the ninja line.
  //
  // The implicit dependencies are the non-linkable deps passed in as an
  // argument, plus the data file depdencies in the target.
  void WriteImplicitDependencies(
      const std::vector<const Target*>& non_linkable_deps);

  Toolchain::ToolType tool_type_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBinaryTargetWriter);
};

#endif  // TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

