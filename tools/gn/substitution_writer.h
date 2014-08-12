// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SUBSTITUTION_WRITER_H_
#define TOOLS_GN_SUBSTITUTION_WRITER_H_

#include <iosfwd>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/substitution_type.h"

struct EscapeOptions;
class OutputFile;
class Settings;
class SourceDir;
class SourceFile;
class SubstitutionList;
class SubstitutionPattern;

// Help text for script source expansion.
extern const char kSourceExpansion_Help[];

// This class handles writing or applying substitution patterns to strings.
class SubstitutionWriter {
 public:
  enum OutputStyle {
    OUTPUT_ABSOLUTE,  // Dirs will be absolute "//foo/bar".
    OUTPUT_RELATIVE,  // Dirs will be relative to a given directory.
  };

  SubstitutionWriter();
  ~SubstitutionWriter();

  // Converts the given SubstitutionList to OutputFiles assuming there are
  // no substitutions (it will assert if there are). This is used for cases
  // like actions where the outputs are explicit, but the list is stored as
  // a SubstitutionList.
  static void GetListAsSourceFiles(
      const Settings* settings,
      const SubstitutionList& list,
      std::vector<SourceFile>* output);
  static void GetListAsOutputFiles(
      const Settings* settings,
      const SubstitutionList& list,
      std::vector<OutputFile>* output);

  // Applies the substitution pattern to a source file, returning the result
  // as either a SourceFile or OutputFile.
  static SourceFile ApplyPatternToSource(
      const Settings* settings,
      const SubstitutionPattern& pattern,
      const SourceFile& source);
  static OutputFile ApplyPatternToSourceAsOutputFile(
      const Settings* settings,
      const SubstitutionPattern& pattern,
      const SourceFile& source);

  // Applies the substitution list to a source, APPENDING the result to the
  // given output vector. It works this way so one can call multiple times to
  // apply to multiple files and create a list. The result can either be
  // SourceFiles or OutputFiles.
  static void ApplyListToSource(
      const Settings* settings,
      const SubstitutionList& list,
      const SourceFile& source,
      std::vector<SourceFile>* output);
  static void ApplyListToSourceAsOutputFile(
      const Settings* settings,
      const SubstitutionList& list,
      const SourceFile& source,
      std::vector<OutputFile>* output);

  // Like ApplyListToSource but applies the list to all sources and replaces
  // rather than appesnds the output (this produces the complete output).
  static void ApplyListToSources(
      const Settings* settings,
      const SubstitutionList& list,
      const std::vector<SourceFile>& sources,
      std::vector<SourceFile>* output);
  static void ApplyListToSourcesAsOutputFile(
      const Settings* settings,
      const SubstitutionList& list,
      const std::vector<SourceFile>& sources,
      std::vector<OutputFile>* output);

  // Given a list of source replacement types used, writes the Ninja variable
  // definitions for the given source file to use for those replacements. The
  // variables will be indented two spaces. Since this is for writing to
  // Ninja files, paths will be relative to the build dir, and no definition
  // for {{source}} will be written since that maps to Ninja's implicit $in
  // variable.
  static void WriteNinjaVariablesForSource(
      const Settings* settings,
      const SourceFile& source,
      const std::vector<SubstitutionType>& types,
      const EscapeOptions& escape_options,
      std::ostream& out);

  // Writes the pattern to the given stream with no special handling, and with
  // Ninja variables replacing the patterns.
  static void WriteWithNinjaVariables(
      const SubstitutionPattern& pattern,
      const EscapeOptions& escape_options,
      std::ostream& out);

  // Extracts the given type of substitution related to a source file from the
  // given source file. If output_style is OUTPUT_RELATIVE, relative_to
  // indicates the directory that the relative directories should be relative
  // to, otherwise it is ignored.
  static std::string GetSourceSubstitution(
      const Settings* settings,
      const SourceFile& source,
      SubstitutionType type,
      OutputStyle output_style,
      const SourceDir& relative_to);
};

#endif  // TOOLS_GN_SUBSTITUTION_WRITER_H_
