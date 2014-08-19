// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

#include "base/compiler_specific.h"
#include "tools/gn/config_values.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/unique_vector.h"

struct EscapeOptions;

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaBinaryTargetWriter : public NinjaTargetWriter {
 public:
  NinjaBinaryTargetWriter(const Target* target, std::ostream& out);
  virtual ~NinjaBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  typedef std::set<OutputFile> OutputFileSet;

  void WriteCompilerVars();
  void WriteSources(std::vector<OutputFile>* object_files);
  void WriteLinkerStuff(const std::vector<OutputFile>& object_files);
  void WriteLinkerFlags();
  void WriteLibs();
  void WriteOutputExtension();
  void WriteSolibs(const std::vector<OutputFile>& solibs);

  // Writes the stamp line for a source set. These are not linked.
  void WriteSourceSetStamp(const std::vector<OutputFile>& object_files);

  // Gets all target dependencies and classifies them, as well as accumulates
  // object files from source sets we need to link.
  void GetDeps(UniqueVector<OutputFile>* extra_object_files,
               UniqueVector<const Target*>* linkable_deps,
               UniqueVector<const Target*>* non_linkable_deps) const;

  // Classifies the dependency as linkable or nonlinkable with the current
  // target, adding it to the appropriate vector. If the dependency is a source
  // set we should link in, the source set's object files will be appended to
  // |extra_object_files|.
  void ClassifyDependency(const Target* dep,
                          UniqueVector<OutputFile>* extra_object_files,
                          UniqueVector<const Target*>* linkable_deps,
                          UniqueVector<const Target*>* non_linkable_deps) const;

  // Writes the implicit dependencies for the link or stamp line. This is
  // the "||" and everything following it on the ninja line.
  //
  // The order-only dependencies are the non-linkable deps passed in as an
  // argument, plus the data file depdencies in the target.
  void WriteOrderOnlyDependencies(
      const UniqueVector<const Target*>& non_linkable_deps);

  // Computes the set of output files resulting from compiling the given source
  // file. If the file can be compiled and the tool exists, fills the outputs in
  // and writes the tool type to computed_tool_type. If the file is not
  // compilable, returns false.
  //
  // The target that the source belongs to is passed as an argument. In the
  // case of linking to source sets, this can be different than the target
  // this class is currently writing.
  //
  // The function can succeed with a "NONE" tool type for object files which are
  // just passed to the output. The output will always be overwritten, not
  // appended to.
  bool GetOutputFilesForSource(const Target* target,
                               const SourceFile& source,
                               Toolchain::ToolType* computed_tool_type,
                               std::vector<OutputFile>* outputs) const;

  const Tool* tool_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBinaryTargetWriter);
};

#endif  // TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

